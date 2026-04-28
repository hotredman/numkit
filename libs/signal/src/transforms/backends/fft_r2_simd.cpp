// libs/signal/src/transforms/backends/fft_r2_simd.cpp
//
// Radix-2 FFT kernel as a standalone Highway translation unit.
// Lives in its own .cpp so MSVC's per-TU inliner budget isn't shared
// with the radix-4 / radix-8 variants — adding a butterfly to the
// joint TU regressed this kernel by 30-75% on sizes it didn't even
// touch (see feedback_fft_msvc_limits memory). With the split each
// kernel inlines cleanly inside its own file.

#include "FftKernels.hpp"

#include <cstddef>
#include <utility>
#include <vector>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/fft_r2_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::signal {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// Base-2 bit-reversal (in-place). Stays inline — small enough to
// duplicate across TUs that need it.
HWY_INLINE void bitReverse2(Complex *buf, std::size_t N)
{
    for (std::size_t i = 1, j = 0; i < N; ++i) {
        std::size_t bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(buf[i], buf[j]);
    }
}

// Radix-2 SIMD body — same as the Phase 8e.2 kernel. Used as the
// fallback for any N that isn't handled by a higher-radix kernel.
void Radix2(Complex *buf, std::size_t N, const Complex *W)
{
    if (N <= 1) return;
    bitReverse2(buf, N);

    std::vector<double>             stageWr(N / 2), stageWi(N / 2);
    const hn::ScalableTag<double>   d;
    const std::size_t               lanes = hn::Lanes(d);

    for (std::size_t len = 2; len <= N; len <<= 1) {
        const std::size_t half = len / 2;
        const std::size_t step = N / len;
        for (std::size_t k = 0; k < half; ++k) {
            stageWr[k] = W[k * step].real();
            stageWi[k] = W[k * step].imag();
        }
        for (std::size_t i = 0; i < N; i += len) {
            std::size_t j = 0;
            if (half >= lanes) {
                for (; j + lanes <= half; j += lanes) {
                    const auto wre = hn::LoadU(d, stageWr.data() + j);
                    const auto wim = hn::LoadU(d, stageWi.data() + j);
                    double *pU = reinterpret_cast<double *>(buf + i + j);
                    double *pV = reinterpret_cast<double *>(buf + i + j + half);
                    hn::Vec<decltype(d)> ure, uim, vlre, vlim;
                    hn::LoadInterleaved2(d, pU, ure, uim);
                    hn::LoadInterleaved2(d, pV, vlre, vlim);
                    const auto vre = hn::NegMulAdd(vlim, wim, hn::Mul(vlre, wre));
                    const auto vim = hn::MulAdd   (vlim, wre, hn::Mul(vlre, wim));
                    hn::StoreInterleaved2(hn::Add(ure, vre), hn::Add(uim, vim), d, pU);
                    hn::StoreInterleaved2(hn::Sub(ure, vre), hn::Sub(uim, vim), d, pV);
                }
            }
            for (; j < half; ++j) {
                const Complex u = buf[i + j];
                const Complex v = buf[i + j + half]
                                  * Complex(stageWr[j], stageWi[j]);
                buf[i + j]        = u + v;
                buf[i + j + half] = u - v;
            }
        }
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::signal
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::signal::detail {

HWY_EXPORT(Radix2);

void fftRadix2Dispatch(Complex *buf, std::size_t N, const Complex *W)
{
    HWY_DYNAMIC_DISPATCH(Radix2)(buf, N, W);
}

} // namespace numkit::signal::detail

#endif // HWY_ONCE
