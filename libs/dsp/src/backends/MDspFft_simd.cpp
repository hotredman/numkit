// libs/dsp/src/backends/MDspFft_simd.cpp
//
// Highway dynamic-dispatch FFT kernel. Works in-place on the
// caller's interleaved complex buffer; no SoA scratch for the data.
// The only scratch is a pair of per-stage contiguous real/imag
// twiddle arrays (O(N/2) doubles total) that let every butterfly
// stage use a plain contiguous hn::Load for its twiddle vector —
// eliminates the per-lane strided gather that made the first draft
// of this kernel slower than scalar.
//
// Butterflies use hn::LoadInterleaved2 / StoreInterleaved2 to do
// the AoS ↔ SoA conversion inline (no whole-buffer split), and an
// FMA-friendly SoA complex multiply:
//   v_re = v_lower_re * w_re - v_lower_im * w_im
//   v_im = v_lower_re * w_im + v_lower_im * w_re
// 2 Mul + 2 FMA per lane-vector, no shuffles.

#include "FftKernels.hpp"

#include <cstddef>
#include <utility>
#include <vector>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/MDspFft_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::m::dsp {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

HWY_INLINE void scalarButterfly(Complex *buf,
                                std::size_t i, std::size_t j,
                                std::size_t half,
                                double wre, double wim)
{
    const Complex u    = buf[i + j];
    const Complex vlo  = buf[i + j + half];
    const Complex v(vlo.real() * wre - vlo.imag() * wim,
                    vlo.real() * wim + vlo.imag() * wre);
    buf[i + j]        = u + v;
    buf[i + j + half] = u - v;
}

void FftRadix2(Complex *buf, std::size_t N, const Complex *W)
{
    if (N <= 1) return;

    // ── Bit-reversal permutation (scalar, swaps whole Complex values) ──
    for (std::size_t i = 1, j = 0; i < N; ++i) {
        std::size_t bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(buf[i], buf[j]);
    }

    // ── Staged SoA twiddle scratch ────────────────────────────────────
    //
    // At stage `len` we need W[k * step] for k in [0, len/2), where
    // step = N/len. Precompute the required twiddles into two
    // contiguous arrays per stage so the SIMD body can hn::Load
    // them directly instead of gathering per lane.
    //
    // We only need scratch wide enough for the largest stage =
    // half at len=N → N/2 entries. Reused across every stage.
    std::vector<double> stageWr(N / 2);
    std::vector<double> stageWi(N / 2);

    const hn::ScalableTag<double> d;
    const std::size_t lanes = hn::Lanes(d);

    for (std::size_t len = 2; len <= N; len <<= 1) {
        const std::size_t half = len / 2;
        const std::size_t step = N / len;

        // Fill stage-local twiddle cache once per stage.
        for (std::size_t k = 0; k < half; ++k) {
            stageWr[k] = W[k * step].real();
            stageWi[k] = W[k * step].imag();
        }
        const double *HWY_RESTRICT pWr = stageWr.data();
        const double *HWY_RESTRICT pWi = stageWi.data();

        for (std::size_t i = 0; i < N; i += len) {
            std::size_t j = 0;
            if (half >= lanes) {
                for (; j + lanes <= half; j += lanes) {
                    // Contiguous twiddle loads — the reason this
                    // kernel exists.
                    const auto wre = hn::Load(d, pWr + j);
                    const auto wim = hn::Load(d, pWi + j);

                    double *pU = reinterpret_cast<double *>(buf + i + j);
                    double *pV = reinterpret_cast<double *>(buf + i + j + half);

                    hn::Vec<decltype(d)> ure, uim;
                    hn::Vec<decltype(d)> vlre, vlim;
                    hn::LoadInterleaved2(d, pU, ure, uim);
                    hn::LoadInterleaved2(d, pV, vlre, vlim);

                    // v = v_lower * w in SoA form (no shuffles, FMA)
                    const auto vre = hn::NegMulAdd(vlim, wim, hn::Mul(vlre, wre));
                    const auto vim = hn::MulAdd   (vlim, wre, hn::Mul(vlre, wim));

                    hn::StoreInterleaved2(hn::Add(ure, vre), hn::Add(uim, vim), d, pU);
                    hn::StoreInterleaved2(hn::Sub(ure, vre), hn::Sub(uim, vim), d, pV);
                }
            }
            // Scalar tail / small-stage fallback.
            for (; j < half; ++j)
                scalarButterfly(buf, i, j, half, pWr[j], pWi[j]);
        }
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::m::dsp
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::m::dsp::detail {

HWY_EXPORT(FftRadix2);

void fftRadix2Impl(Complex *buf, std::size_t N, const Complex *W)
{
    HWY_DYNAMIC_DISPATCH(FftRadix2)(buf, N, W);
}

} // namespace numkit::m::dsp::detail

#endif // HWY_ONCE
