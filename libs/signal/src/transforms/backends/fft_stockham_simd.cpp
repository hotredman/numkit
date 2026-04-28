// libs/signal/src/transforms/backends/fft_stockham_simd.cpp
//
// Stockham auto-sort radix-2 FFT. Out-of-place (ping-pong between two
// buffers) — eliminates the bit-reversal pass that radix-2 in-place
// (Cooley-Tukey iterative) needs at the start. The classic auto-sort
// formulation: at stage s producing size-m sub-FFTs from size-(m/2)
// inputs, output sub-FFTs are stored consecutively in dst and read
// from corresponding pairs in src.
//
// Why a separate TU: same MSVC inliner-budget reason as the r2 / r4
// split (see feedback_fft_msvc_limits memory). Including the Stockham
// butterfly alongside the existing kernels would regress them.
//
// When does this win over the in-place r2 kernel?
//
//   * Bit-reverse-pass is gone — saves one full sweep over the data
//     plus its scattered access pattern (cache-unfriendly because
//     bit-reversed swaps touch wildly different cache lines).
//
//   * Twiddle-table access stays sequential and SIMD-aligned across
//     the inner k loop (same as r2 — we pre-strided twiddles per
//     stage into stageWr/stageWi for both kernels).
//
//   * Two-buffer working set (2 × N complex) is bigger than in-place
//     N. At N where 2N still fits L2/L3 the bit-reversal saving wins;
//     at N large enough that 2N spills past L3 to DRAM, the extra
//     bandwidth might cost. Empirical crossover is at the dispatcher
//     in fft_simd.cpp.

#include "fft_kernels.hpp"

#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/fft_stockham_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::signal {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

HWY_INLINE std::size_t ilog2(std::size_t N)
{
    std::size_t k = 0;
    for (std::size_t n = N; n > 1; n >>= 1) ++k;
    return k;
}

// Stockham auto-sort radix-2. `x` is BOTH input and output buffer.
// `scratch` is a working buffer of size N (caller-provided to amortise
// the allocation across calls). `W` is the forward-direction twiddle
// table of length N/2.
//
// At each stage the code prepares a stage-local twiddle table
// (stageWr/stageWi) sized mh = m/2, then iterates over n_sub pairs of
// input sub-FFTs. The inner k loop is SIMD-vectorised when mh ≥ lanes,
// loading mh consecutive (twiddle, src_a, src_b) triples at a time.
void Stockham(Complex *x, Complex *scratch, std::size_t N, const Complex *W)
{
    if (N <= 1) return;

    std::vector<double>             stageWr(N / 2), stageWi(N / 2);
    const hn::ScalableTag<double>   d;
    const std::size_t               lanes = hn::Lanes(d);

    Complex *src = x;
    Complex *dst = scratch;

    const std::size_t L = ilog2(N);

    for (std::size_t stage = 0; stage < L; ++stage) {
        const std::size_t m     = std::size_t(1) << (stage + 1); // out sub-FFT size
        const std::size_t mh    = m >> 1;                        // half = stage's input sub-FFT size
        const std::size_t n_sub = N / m;                          // number of output sub-FFTs
        const std::size_t step  = n_sub;                          // twiddle stride

        // Pre-stride the twiddles into contiguous arrays so the SIMD
        // load is aligned and unit-stride. Same pattern as the r2 TU.
        for (std::size_t k = 0; k < mh; ++k) {
            stageWr[k] = W[k * step].real();
            stageWi[k] = W[k * step].imag();
        }

        for (std::size_t pair_idx = 0; pair_idx < n_sub; ++pair_idx) {
            // Auto-sort layout: at this stage, src holds 2*n_sub sub-FFTs
            // of size mh. The pair contributing to OUTPUT sub-FFT
            // pair_idx is the input sub-FFT pair (pair_idx, pair_idx + n_sub)
            // — i.e. their bases are EXACTLY N/2 apart in memory. NOT
            // (2*pair_idx, 2*pair_idx + 1) — that would be in-place
            // radix-2 (Cooley-Tukey) layout. The N/2 stride is FIXED
            // across all stages (in_stride = N/2 always); only the
            // chunk size mh inside each sub-FFT changes. This stride-
            // invariance is exactly what gives Stockham its "auto-sort"
            // property: bit-reversal disappears.
            //
            // a_in/b_in non-const so reinterpret_cast<double*> for
            // Highway's LoadInterleaved2 doesn't trip the qualifier
            // check; the kernel only reads through these pointers.
            Complex *a_in = src + pair_idx           * mh;
            Complex *b_in = src + (pair_idx + n_sub) * mh;
                                  // = a_in + N/2

            // dst layout: n_sub sub-FFTs of size m, contiguously.
            // dst[pair_idx * m + k]      = first half of output sub-FFT
            // dst[pair_idx * m + mh + k] = second half
            Complex       *a_out = dst + pair_idx * m;
            Complex       *b_out = a_out + mh;

            std::size_t k = 0;
            if (mh >= lanes) {
                for (; k + lanes <= mh; k += lanes) {
                    const auto wre = hn::LoadU(d, stageWr.data() + k);
                    const auto wim = hn::LoadU(d, stageWi.data() + k);

                    double *pA = reinterpret_cast<double *>(a_in + k);
                    double *pB = reinterpret_cast<double *>(b_in + k);
                    hn::Vec<decltype(d)> are, aim, bre, bim;
                    hn::LoadInterleaved2(d, pA, are, aim);
                    hn::LoadInterleaved2(d, pB, bre, bim);

                    // bv = b * w  (complex multiply)
                    const auto bvre = hn::NegMulAdd(bim, wim, hn::Mul(bre, wre));
                    const auto bvim = hn::MulAdd   (bim, wre, hn::Mul(bre, wim));

                    double *pAOut = reinterpret_cast<double *>(a_out + k);
                    double *pBOut = reinterpret_cast<double *>(b_out + k);
                    hn::StoreInterleaved2(hn::Add(are, bvre), hn::Add(aim, bvim),
                                          d, pAOut);
                    hn::StoreInterleaved2(hn::Sub(are, bvre), hn::Sub(aim, bvim),
                                          d, pBOut);
                }
            }
            for (; k < mh; ++k) {
                const Complex a = a_in[k];
                const Complex b = b_in[k] * Complex(stageWr[k], stageWi[k]);
                a_out[k] = a + b;
                b_out[k] = a - b;
            }
        }

        std::swap(src, dst);
    }

    // After L stages, src holds the result. If src points to scratch
    // (odd L), copy it back into x. Even L means src == x already.
    if (src != x)
        std::memcpy(x, src, N * sizeof(Complex));
}

} // namespace HWY_NAMESPACE
} // namespace numkit::signal
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::signal::detail {

HWY_EXPORT(Stockham);

// Same thread-local-scratch pattern as the FFT wrapper's working
// buffer (see fft.cpp). One persistent allocation per thread,
// grows monotonically across calls. Lifetime tied to the thread.
void fftStockhamDispatch(Complex *buf, std::size_t N, const Complex *W)
{
    thread_local std::vector<Complex> scratch;
    if (scratch.size() < N)
        scratch.resize(N);
    HWY_DYNAMIC_DISPATCH(Stockham)(buf, scratch.data(), N, W);
}

} // namespace numkit::signal::detail

#endif // HWY_ONCE
