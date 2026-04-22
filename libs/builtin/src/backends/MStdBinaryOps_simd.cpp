// libs/builtin/src/backends/MStdBinaryOps_simd.cpp
//
// Highway dynamic-dispatch inner loops for plus / minus / times /
// rdivide on real double arrays. One HWY_EXPORT / HWY_DYNAMIC_DISPATCH
// pair per op — the public entry points in the same detail namespace
// forward to the dispatcher. The surrounding public plus() / minus()
// / times() / rdivide() in MStdBinaryOps.cpp are unchanged; they
// call these loops only for the 2D same-shape DOUBLE fast path.

#include "BinaryOpsLoops.hpp"

#include <numkit/m/core/MParallelFor.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <hwy/cache_control.h>  // hwy::FlushStream() — sfence on x86

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/MStdBinaryOps_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::m::builtin {
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

// Vectorised SAXPY matrix multiply (column-major). Loop order matches
// the portable scalar reference — identical k-reduction order means the
// only numerical divergence comes from MulAdd being a fused op on
// FMA-capable targets (AVX2 and later), which is at most 0.5 ULP per
// accumulation tighter than scalar mul-then-add.
void MatmulLoop(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,
                double *HWY_RESTRICT c, std::size_t M, std::size_t N, std::size_t K)
{
    const hn::ScalableTag<double> d;
    const std::size_t lanes = hn::Lanes(d);

    for (std::size_t j = 0; j < N; ++j) {
        double *c_col = c + j * M;
        // Zero the output column in SIMD-wide stores.
        {
            const auto zero = hn::Zero(d);
            std::size_t i = 0;
            for (; i + lanes <= M; i += lanes) hn::StoreU(zero, d, c_col + i);
            for (; i < M; ++i) c_col[i] = 0.0;
        }

        for (std::size_t k = 0; k < K; ++k) {
            const double bkj = b[j * K + k];
            const auto vb = hn::Set(d, bkj);
            const double *a_col = a + k * M;

            std::size_t i = 0;
            for (; i + lanes <= M; i += lanes) {
                const auto va = hn::LoadU(d, a_col + i);
                const auto vc = hn::LoadU(d, c_col + i);
                hn::StoreU(hn::MulAdd(va, vb, vc), d, c_col + i);
            }
            for (; i < M; ++i) c_col[i] += bkj * a_col[i];
        }
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::m::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::m::builtin::detail {

HWY_EXPORT(PlusLoop);
HWY_EXPORT(MinusLoop);
HWY_EXPORT(TimesLoop);
HWY_EXPORT(RdivideLoop);
HWY_EXPORT(MatmulLoop);

// Each dispatcher wraps the per-target SIMD body in a parallel_for so
// big arrays split across worker threads. Each thread calls the same
// per-target loop on its disjoint [start, end) slice — output buffers
// don't overlap, so per-element semantics are bit-identical to the
// single-threaded path. Below kElementwiseThreshold (and on builds
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
    numkit::m::detail::parallel_for(n, numkit::m::detail::kElementwiseThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(PlusLoop)(a + s, b + s, out + s, e - s);
        }, numkit::m::detail::kElementwiseMaxWorkers);
    hwy::FlushStream();
}

void minusLoop(const double *a, const double *b, double *out, std::size_t n)
{
    numkit::m::detail::parallel_for(n, numkit::m::detail::kElementwiseThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(MinusLoop)(a + s, b + s, out + s, e - s);
        }, numkit::m::detail::kElementwiseMaxWorkers);
    hwy::FlushStream();
}

void timesLoop(const double *a, const double *b, double *out, std::size_t n)
{
    numkit::m::detail::parallel_for(n, numkit::m::detail::kElementwiseThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(TimesLoop)(a + s, b + s, out + s, e - s);
        }, numkit::m::detail::kElementwiseMaxWorkers);
    hwy::FlushStream();
}

void rdivideLoop(const double *a, const double *b, double *out, std::size_t n)
{
    numkit::m::detail::parallel_for(n, numkit::m::detail::kElementwiseThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(RdivideLoop)(a + s, b + s, out + s, e - s);
        }, numkit::m::detail::kElementwiseMaxWorkers);
    hwy::FlushStream();
}

void matmulDoubleLoop(const double *a, const double *b, double *c,
                      std::size_t M, std::size_t N, std::size_t K)
{
    HWY_DYNAMIC_DISPATCH(MatmulLoop)(a, b, c, M, N, K);
}

} // namespace numkit::m::builtin::detail

#endif // HWY_ONCE
