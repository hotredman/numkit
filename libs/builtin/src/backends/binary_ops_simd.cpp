// libs/builtin/src/backends/binary_ops_simd.cpp
//
// Highway dynamic-dispatch inner loops for plus / minus / times /
// rdivide on real double arrays. One HWY_EXPORT / HWY_DYNAMIC_DISPATCH
// pair per op — the public entry points in the same detail namespace
// forward to the dispatcher. The surrounding public plus() / minus()
// / times() / rdivide() in binary_ops.cpp are unchanged; they
// call these loops only for the 2D same-shape DOUBLE fast path.

#include "BinaryOpsLoops.hpp"

#include <numkit/core/parallel_for.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <hwy/cache_control.h>  // hwy::FlushStream() — sfence on x86

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/binary_ops_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// ── Element-wise binary kernels with non-temporal stores ────────────
//
// Big-N elementwise ops (`z = x + y` and friends) are pure write-once
// memory streaming: 16 MB read + 8 MB write at N=1M, kernel runs
// once, output is consumed by something else and never read back
// in this loop. NT-stores (`vmovntpd` on x86, equivalent on other
// ISAs via Highway) write straight to memory bypassing the cache:
//   * saves the 8 MB write-allocate that a normal store would do,
//     dropping effective transfer from 24 MB to 16 MB (≈1.5×);
//   * doesn't pollute L1/L2/L3 with the output, keeping room for
//     x and y if the caller reuses them on the next iteration.
//
// Caveat: `hn::Stream` requires the destination pointer to be
// aligned to N*sizeof(double). The default Allocator/HeapAlloc
// only guarantees max_align_t (16 B on MSVC), and parallel_for
// hands each worker an arbitrary [start, end) slice — so
// `out + start` may not be SIMD-aligned. The kernels below
// process an unaligned head with StoreU until aligned, then
// the body with Stream, then a scalar tail. After Stream we
// issue hwy::FlushStream() (sfence on x86) so subsequent loads
// observe the writes — NT stores are weakly ordered. On
// architectures without NT-stores (WASM SIMD128) Highway falls
// back to plain store and FlushStream is a no-op.
namespace {

// Returns the number of leading scalar elements needed to bring
// `out + i` to N*sizeof(double) alignment.
HWY_INLINE std::size_t alignmentLead(const double *out,
                                     std::size_t i, std::size_t align_bytes)
{
    const auto addr = reinterpret_cast<std::uintptr_t>(out + i);
    const auto rem  = addr % align_bytes;
    if (rem == 0) return 0;
    return (align_bytes - rem) / sizeof(double);
}

} // namespace

// 4× unrolled body — issues four independent Stream stores per
// iteration so the CPU keeps four loads + four ops + four stores
// in flight, hiding load latency. The remainder (less than 4
// vectors) goes through a tight 1-vector loop. Same NT-store +
// alignment rules as the head; FlushStream is dispatcher-level.
#define NK_BINOP_LOOP(NAME, VEC_OP, SCALAR_OP)                                            \
    void NAME(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,                 \
              double *HWY_RESTRICT out, std::size_t n)                                    \
    {                                                                                     \
        const hn::ScalableTag<double> d;                                                  \
        const std::size_t N           = hn::Lanes(d);                                     \
        const std::size_t align_bytes = N * sizeof(double);                               \
                                                                                          \
        std::size_t i = 0;                                                                \
        const std::size_t head = std::min(alignmentLead(out, i, align_bytes), n);         \
        for (std::size_t k = 0; k < head; ++k, ++i) out[i] = a[i] SCALAR_OP b[i];         \
                                                                                          \
        const std::size_t step4 = 4 * N;                                                  \
        for (; i + step4 <= n; i += step4) {                                              \
            hn::Stream(hn::VEC_OP(hn::LoadU(d, a + i + 0 * N),                            \
                                  hn::LoadU(d, b + i + 0 * N)), d, out + i + 0 * N);     \
            hn::Stream(hn::VEC_OP(hn::LoadU(d, a + i + 1 * N),                            \
                                  hn::LoadU(d, b + i + 1 * N)), d, out + i + 1 * N);     \
            hn::Stream(hn::VEC_OP(hn::LoadU(d, a + i + 2 * N),                            \
                                  hn::LoadU(d, b + i + 2 * N)), d, out + i + 2 * N);     \
            hn::Stream(hn::VEC_OP(hn::LoadU(d, a + i + 3 * N),                            \
                                  hn::LoadU(d, b + i + 3 * N)), d, out + i + 3 * N);     \
        }                                                                                 \
        for (; i + N <= n; i += N)                                                        \
            hn::Stream(hn::VEC_OP(hn::LoadU(d, a + i), hn::LoadU(d, b + i)),              \
                       d, out + i);                                                       \
        for (; i < n; ++i) out[i] = a[i] SCALAR_OP b[i];                                  \
    }

NK_BINOP_LOOP(PlusLoop,    Add, +)
NK_BINOP_LOOP(MinusLoop,   Sub, -)
NK_BINOP_LOOP(TimesLoop,   Mul, *)
NK_BINOP_LOOP(RdivideLoop, Div, /)

#undef NK_BINOP_LOOP

// Matrix multiply (column-major). Three paths:
//
// * Single-pass register-tiling kernel for K ≤ 2*KC (small/medium K).
//   8×4 MR×NR tile of C kept in 8 YMM accumulators; the inner k loop
//   sweeps the whole [0, K) range, accumulating without touching C
//   memory until the final store. With K ≤ 512, the (i0, j0) tile's
//   working set still fits L1 well enough that L1-residency-of-B
//   doesn't pay for the extra C-tile Load+Store overhead of K-blocking.
//
// * K-blocked register-tiling kernel for K > 2*KC (large K).
//   Splits the inner k loop into KC-sized chunks. First chunk
//   zero-inits accumulators in registers; subsequent chunks Load
//   existing partial sums from C, accumulate, Store back.
//
//   Why K-blocking helps at large K: the inner k loop now touches an
//   A panel of MR×KC and a B panel of KC×NR. Sized KC=256, those
//   are 16 KB + 8 KB = 24 KB — fit in L1d (48 KB). For fixed
//   (j0, kb), sweeping i0 reuses the same B panel out of L1 across
//   M/MR iterations; without K-blocking, A panel for one (i0, j0)
//   alone is K×MR×8 = 64 KB at K=1024, already exceeding L1, so B
//   gets evicted between i0 iterations and re-fetched from L2/L3.
//   At 1024² this halves DRAM/L3 bandwidth on B, which is what was
//   capping throughput at 158 GF (vs 270 GF on the 512² case that
//   stays L2-resident even without K-blocking).
//
// * Original SAXPY-down-the-column path for the tail (rows below
//   M_body and cols past N_body) and for problems too small to tile.
//
// The two register-tile paths live in HWY_NOINLINE helpers to keep
// MSVC's inliner from sharing budget across them — when both bodies
// were inlined into one function, the K=512 single-pass case
// regressed 15% from MSVC running out of optimisation budget on the
// then-larger function (same cliff documented in
// feedback_fft_msvc_limits memory). Splitting into separate
// functions eliminated the regression.
//
// All numeric semantics match the original kernel: identical (j, k, i)
// reduction order so FMA rounding stays bit-identical between the
// two paths. On a square 512×512 mtimes the body covers the whole
// matrix (512 % 8 == 0, 512 % 4 == 0) so the tail path stays cold.

namespace {

constexpr std::size_t kMR = 8;   // rows per tile (fits 2 vec of 4 lanes)
constexpr std::size_t kNR = 4;   // cols per tile (4 outer-product accumulators)
constexpr std::size_t kKC = 256; // K-block — A panel + B panel fit L1

// Single-pass body: zero-init in registers, sweep all of K, store.
// Used for K ≤ 2*KC. Operates on the body region [0, M_body) × [0, N_body)
// — caller handles tail rows/cols via saxpyRange. Left inline (no
// HWY_NOINLINE) so MSVC matches the pre-K-blocking codegen exactly
// for small/medium K; only matmulKBlocked is forced NOINLINE to keep
// the inliner from regressing this path with the heavier K-blocked one.
void matmulSinglePass(const double *HWY_RESTRICT a,
                                   const double *HWY_RESTRICT b,
                                   double *HWY_RESTRICT c,
                                   std::size_t M, std::size_t K,
                                   std::size_t M_body, std::size_t N_body)
{
    const hn::ScalableTag<double> d;
    const std::size_t MR_HALF = hn::Lanes(d);

    for (std::size_t j0 = 0; j0 < N_body; j0 += kNR) {
        for (std::size_t i0 = 0; i0 < M_body; i0 += kMR) {
            double *c_tile = c + i0;
            auto c00 = hn::Zero(d), c10 = hn::Zero(d);
            auto c01 = hn::Zero(d), c11 = hn::Zero(d);
            auto c02 = hn::Zero(d), c12 = hn::Zero(d);
            auto c03 = hn::Zero(d), c13 = hn::Zero(d);

            for (std::size_t k = 0; k < K; ++k) {
                const double *a_col = a + k * M;
                const auto    a0 = hn::LoadU(d, a_col + i0);
                const auto    a1 = hn::LoadU(d, a_col + i0 + MR_HALF);
                const auto    b0 = hn::Set(d, b[(j0 + 0) * K + k]);
                const auto    b1 = hn::Set(d, b[(j0 + 1) * K + k]);
                const auto    b2 = hn::Set(d, b[(j0 + 2) * K + k]);
                const auto    b3 = hn::Set(d, b[(j0 + 3) * K + k]);

                c00 = hn::MulAdd(a0, b0, c00);
                c10 = hn::MulAdd(a1, b0, c10);
                c01 = hn::MulAdd(a0, b1, c01);
                c11 = hn::MulAdd(a1, b1, c11);
                c02 = hn::MulAdd(a0, b2, c02);
                c12 = hn::MulAdd(a1, b2, c12);
                c03 = hn::MulAdd(a0, b3, c03);
                c13 = hn::MulAdd(a1, b3, c13);
            }

            hn::StoreU(c00, d, c_tile + (j0 + 0) * M);
            hn::StoreU(c10, d, c_tile + (j0 + 0) * M + MR_HALF);
            hn::StoreU(c01, d, c_tile + (j0 + 1) * M);
            hn::StoreU(c11, d, c_tile + (j0 + 1) * M + MR_HALF);
            hn::StoreU(c02, d, c_tile + (j0 + 2) * M);
            hn::StoreU(c12, d, c_tile + (j0 + 2) * M + MR_HALF);
            hn::StoreU(c03, d, c_tile + (j0 + 3) * M);
            hn::StoreU(c13, d, c_tile + (j0 + 3) * M + MR_HALF);
        }
    }
}

// K-blocked body. First k-block zero-inits + accumulates KC; subsequent
// k-blocks Load existing partial sums + accumulate + Store back.
// Used for K > 2*KC. Same body region contract as matmulSinglePass.
HWY_NOINLINE void matmulKBlocked(const double *HWY_RESTRICT a,
                                 const double *HWY_RESTRICT b,
                                 double *HWY_RESTRICT c,
                                 std::size_t M, std::size_t K,
                                 std::size_t M_body, std::size_t N_body)
{
    const hn::ScalableTag<double> d;
    const std::size_t MR_HALF = hn::Lanes(d);

    // First k-block: k in [0, KC). Zero-init accumulators, store at end.
    for (std::size_t j0 = 0; j0 < N_body; j0 += kNR) {
        for (std::size_t i0 = 0; i0 < M_body; i0 += kMR) {
            double *c_tile = c + i0;
            auto c00 = hn::Zero(d), c10 = hn::Zero(d);
            auto c01 = hn::Zero(d), c11 = hn::Zero(d);
            auto c02 = hn::Zero(d), c12 = hn::Zero(d);
            auto c03 = hn::Zero(d), c13 = hn::Zero(d);

            for (std::size_t k = 0; k < kKC; ++k) {
                const double *a_col = a + k * M;
                const auto    a0 = hn::LoadU(d, a_col + i0);
                const auto    a1 = hn::LoadU(d, a_col + i0 + MR_HALF);
                const auto    b0 = hn::Set(d, b[(j0 + 0) * K + k]);
                const auto    b1 = hn::Set(d, b[(j0 + 1) * K + k]);
                const auto    b2 = hn::Set(d, b[(j0 + 2) * K + k]);
                const auto    b3 = hn::Set(d, b[(j0 + 3) * K + k]);

                c00 = hn::MulAdd(a0, b0, c00);
                c10 = hn::MulAdd(a1, b0, c10);
                c01 = hn::MulAdd(a0, b1, c01);
                c11 = hn::MulAdd(a1, b1, c11);
                c02 = hn::MulAdd(a0, b2, c02);
                c12 = hn::MulAdd(a1, b2, c12);
                c03 = hn::MulAdd(a0, b3, c03);
                c13 = hn::MulAdd(a1, b3, c13);
            }

            hn::StoreU(c00, d, c_tile + (j0 + 0) * M);
            hn::StoreU(c10, d, c_tile + (j0 + 0) * M + MR_HALF);
            hn::StoreU(c01, d, c_tile + (j0 + 1) * M);
            hn::StoreU(c11, d, c_tile + (j0 + 1) * M + MR_HALF);
            hn::StoreU(c02, d, c_tile + (j0 + 2) * M);
            hn::StoreU(c12, d, c_tile + (j0 + 2) * M + MR_HALF);
            hn::StoreU(c03, d, c_tile + (j0 + 3) * M);
            hn::StoreU(c13, d, c_tile + (j0 + 3) * M + MR_HALF);
        }
    }

    // Subsequent k-blocks: k in [k0, k_hi). Load existing C tile,
    // accumulate, store back. B[kb..kb+KC, j0..j0+3] stays L1-resident
    // across the M/MR i0 sweep — the access pattern that makes
    // K-blocking pay off.
    for (std::size_t k0 = kKC; k0 < K; k0 += kKC) {
        const std::size_t k_hi = (k0 + kKC < K) ? k0 + kKC : K;

        for (std::size_t j0 = 0; j0 < N_body; j0 += kNR) {
            for (std::size_t i0 = 0; i0 < M_body; i0 += kMR) {
                double *c_tile = c + i0;
                auto c00 = hn::LoadU(d, c_tile + (j0 + 0) * M);
                auto c10 = hn::LoadU(d, c_tile + (j0 + 0) * M + MR_HALF);
                auto c01 = hn::LoadU(d, c_tile + (j0 + 1) * M);
                auto c11 = hn::LoadU(d, c_tile + (j0 + 1) * M + MR_HALF);
                auto c02 = hn::LoadU(d, c_tile + (j0 + 2) * M);
                auto c12 = hn::LoadU(d, c_tile + (j0 + 2) * M + MR_HALF);
                auto c03 = hn::LoadU(d, c_tile + (j0 + 3) * M);
                auto c13 = hn::LoadU(d, c_tile + (j0 + 3) * M + MR_HALF);

                for (std::size_t k = k0; k < k_hi; ++k) {
                    const double *a_col = a + k * M;
                    const auto    a0 = hn::LoadU(d, a_col + i0);
                    const auto    a1 = hn::LoadU(d, a_col + i0 + MR_HALF);
                    const auto    b0 = hn::Set(d, b[(j0 + 0) * K + k]);
                    const auto    b1 = hn::Set(d, b[(j0 + 1) * K + k]);
                    const auto    b2 = hn::Set(d, b[(j0 + 2) * K + k]);
                    const auto    b3 = hn::Set(d, b[(j0 + 3) * K + k]);

                    c00 = hn::MulAdd(a0, b0, c00);
                    c10 = hn::MulAdd(a1, b0, c10);
                    c01 = hn::MulAdd(a0, b1, c01);
                    c11 = hn::MulAdd(a1, b1, c11);
                    c02 = hn::MulAdd(a0, b2, c02);
                    c12 = hn::MulAdd(a1, b2, c12);
                    c03 = hn::MulAdd(a0, b3, c03);
                    c13 = hn::MulAdd(a1, b3, c13);
                }

                hn::StoreU(c00, d, c_tile + (j0 + 0) * M);
                hn::StoreU(c10, d, c_tile + (j0 + 0) * M + MR_HALF);
                hn::StoreU(c01, d, c_tile + (j0 + 1) * M);
                hn::StoreU(c11, d, c_tile + (j0 + 1) * M + MR_HALF);
                hn::StoreU(c02, d, c_tile + (j0 + 2) * M);
                hn::StoreU(c12, d, c_tile + (j0 + 2) * M + MR_HALF);
                hn::StoreU(c03, d, c_tile + (j0 + 3) * M);
                hn::StoreU(c13, d, c_tile + (j0 + 3) * M + MR_HALF);
            }
        }
    }
}

} // namespace

void MatmulLoop(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,
                double *HWY_RESTRICT c, std::size_t M, std::size_t N, std::size_t K)
{
    const hn::ScalableTag<double> d;
    const std::size_t lanes = hn::Lanes(d);

    // Helper: SAXPY-down-column path for an arbitrary [j_lo, j_hi) col
    // range and an arbitrary [i_lo, i_hi) row range. Used for the tail
    // and for problems too small (or wrong vector width) for the
    // 8×4 register tile.
    auto saxpyRange = [&](std::size_t i_lo, std::size_t i_hi,
                          std::size_t j_lo, std::size_t j_hi) {
        for (std::size_t j = j_lo; j < j_hi; ++j) {
            double *c_col = c + j * M;
            {
                const auto zero = hn::Zero(d);
                std::size_t i = i_lo;
                for (; i + lanes <= i_hi; i += lanes) hn::StoreU(zero, d, c_col + i);
                for (; i < i_hi; ++i) c_col[i] = 0.0;
            }
            for (std::size_t k = 0; k < K; ++k) {
                const double bkj = b[j * K + k];
                const auto vb = hn::Set(d, bkj);
                const double *a_col = a + k * M;
                std::size_t i = i_lo;
                for (; i + lanes <= i_hi; i += lanes) {
                    const auto va = hn::LoadU(d, a_col + i);
                    const auto vc = hn::LoadU(d, c_col + i);
                    hn::StoreU(hn::MulAdd(va, vb, vc), d, c_col + i);
                }
                for (; i < i_hi; ++i) c_col[i] += bkj * a_col[i];
            }
        }
    };

    // Lanes other than 4 (e.g. SSE2 = 2 doubles, AVX-512 = 8) need a
    // different MR. For those targets just use the SAXPY path — the
    // 8×4 tile sized for AVX2 wouldn't keep the registers busy on
    // narrower SIMD or would overflow them on wider.
    if (lanes != 4 || M < kMR || N < kNR) {
        saxpyRange(0, M, 0, N);
        return;
    }

    const std::size_t M_body = (M / kMR) * kMR;
    const std::size_t N_body = (N / kNR) * kNR;

    // Pick the body specialisation. Empirical break-even on Arrow Lake
    // (AVX2 + MSVC + Highway): K=512 ties, K=1024 K-blocked is 1.7×
    // faster, K=2048 is 2.5× faster.
    if (K <= 2 * kKC)
        matmulSinglePass(a, b, c, M, K, M_body, N_body);
    else
        matmulKBlocked(a, b, c, M, K, M_body, N_body);

    // Tail rows (i in [M_body, M)) for the body cols. saxpyRange
    // does its own zero-and-accumulate over full K, independent of
    // the body's K-block sweep — its writes go to disjoint C rows.
    if (M_body < M)
        saxpyRange(M_body, M, 0, N_body);
    // Tail cols (j in [N_body, N)) for all rows.
    if (N_body < N)
        saxpyRange(0, M, N_body, N);
}

} // namespace HWY_NAMESPACE
} // namespace numkit::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::builtin::detail {

HWY_EXPORT(PlusLoop);
HWY_EXPORT(MinusLoop);
HWY_EXPORT(TimesLoop);
HWY_EXPORT(RdivideLoop);
HWY_EXPORT(MatmulLoop);

// Each dispatcher wraps the per-target SIMD body in a parallel_for so
// big arrays split across worker threads. Each thread calls the same
// per-target loop on its disjoint [start, end) slice — output buffers
// don't overlap, so per-element semantics are bit-identical to the
// single-threaded path. Below kCheapElementwiseThreshold (and on builds
// without NUMKIT_WITH_THREADS) parallel_for runs `fn(0, n)` directly,
// preserving the prior fast path.

// One global sfence per dispatcher call (rather than per parallel
// chunk) keeps the cost amortised across the whole kernel. NT
// stores are weakly ordered; without the fence, a subsequent
// load might see stale data. On non-x86 (and on architectures
// where Highway's Stream falls back to a normal store) FlushStream
// is a no-op.

void plusLoop(const double *a, const double *b, double *out, std::size_t n)
{
    numkit::detail::parallel_for(n, numkit::detail::kCheapElementwiseThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(PlusLoop)(a + s, b + s, out + s, e - s);
        }, numkit::detail::kElementwiseMaxWorkers);
    hwy::FlushStream();
}

void minusLoop(const double *a, const double *b, double *out, std::size_t n)
{
    numkit::detail::parallel_for(n, numkit::detail::kCheapElementwiseThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(MinusLoop)(a + s, b + s, out + s, e - s);
        }, numkit::detail::kElementwiseMaxWorkers);
    hwy::FlushStream();
}

void timesLoop(const double *a, const double *b, double *out, std::size_t n)
{
    numkit::detail::parallel_for(n, numkit::detail::kCheapElementwiseThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(TimesLoop)(a + s, b + s, out + s, e - s);
        }, numkit::detail::kElementwiseMaxWorkers);
    hwy::FlushStream();
}

void rdivideLoop(const double *a, const double *b, double *out, std::size_t n)
{
    numkit::detail::parallel_for(n, numkit::detail::kCheapElementwiseThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(RdivideLoop)(a + s, b + s, out + s, e - s);
        }, numkit::detail::kElementwiseMaxWorkers);
    hwy::FlushStream();
}

// Threshold for parallel matmul. Below this many output columns the
// per-worker overhead overshadows the kernel — the existing single-
// thread MatmulLoop is faster for small N.
inline constexpr std::size_t kMatmulParallelThreshold = 32;

void matmulDoubleLoop(const double *a, const double *b, double *c,
                      std::size_t M, std::size_t N, std::size_t K)
{
    // Outer loop is over columns of C. Each output column is computed
    // from A (read-only) and one column of B; columns are independent,
    // so partitioning [0, N) across workers parallelises the kernel
    // with zero synchronisation. B and C pointers are advanced to the
    // worker's column range (column-major: stride K for B, M for C).
    //
    // The per-target SIMD loop below handles the inner SAXPY; here we
    // just slice the work. Below kMatmulParallelThreshold we skip the
    // pool entirely.
    if (N < kMatmulParallelThreshold) {
        HWY_DYNAMIC_DISPATCH(MatmulLoop)(a, b, c, M, N, K);
        return;
    }
    numkit::detail::parallel_for(N, kMatmulParallelThreshold,
        [=](std::size_t j_start, std::size_t j_end) {
            HWY_DYNAMIC_DISPATCH(MatmulLoop)(
                a,
                b + j_start * K,
                c + j_start * M,
                M, j_end - j_start, K);
        });
}

} // namespace numkit::builtin::detail

#endif // HWY_ONCE
