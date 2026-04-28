// libs/signal/src/backends/fft_r4_simd.cpp
//
// Radix-4 FFT kernel for sizes that are clean powers of 4. Same
// per-TU isolation rationale as fft_r2_simd.cpp — keeping the
// r4 butterfly and its base-4 digit-reversal away from the r2 / r8
// kernels lets MSVC inline within each file's budget.

#include "FftKernels.hpp"

#include <cstddef>
#include <utility>
#include <vector>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/fft_r4_simd.cpp"
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

// Base-4 digit-reversal (in-place). Required because the r4
// butterfly takes inputs in digit-reversed order.
HWY_INLINE void digitReverse4(Complex *buf, std::size_t N)
{
    const std::size_t k = ilog2(N) / 2;
    for (std::size_t i = 0; i < N; ++i) {
        std::size_t j = 0;
        std::size_t x = i;
        for (std::size_t d = 0; d < k; ++d) {
            j = (j << 2) | (x & 3);
            x >>= 2;
        }
        if (i < j) std::swap(buf[i], buf[j]);
    }
}

// Radix-4 stage. See full butterfly derivation in the original
// fft_simd.cpp before the TU split — semantics unchanged.
HWY_NOINLINE void radix4Stage(Complex *buf, std::size_t N, std::size_t len,
                              const Complex *W, std::size_t N_half,
                              double *sW1r, double *sW1i,
                              double *sW2r, double *sW2i,
                              double *sW3r, double *sW3i)
{
    const hn::ScalableTag<double> d;
    const std::size_t lanes = hn::Lanes(d);
    const std::size_t m     = len / 4;
    const std::size_t step  = N / len;

    for (std::size_t k = 0; k < m; ++k) {
        const Complex w1 = W[k * step];
        const Complex w2 = W[2 * k * step];
        const std::size_t i3 = 3 * k * step;
        const Complex w3 = (i3 < N_half)
                           ? W[i3]
                           : Complex(-W[i3 - N_half].real(),
                                     -W[i3 - N_half].imag());
        sW1r[k] = w1.real(); sW1i[k] = w1.imag();
        sW2r[k] = w2.real(); sW2i[k] = w2.imag();
        sW3r[k] = w3.real(); sW3i[k] = w3.imag();
    }

    for (std::size_t i = 0; i < N; i += len) {
        std::size_t j = 0;
        if (m >= lanes) {
            for (; j + lanes <= m; j += lanes) {
                double *pU = reinterpret_cast<double *>(buf + i + j);
                double *pV = reinterpret_cast<double *>(buf + i + j +     m);
                double *pW = reinterpret_cast<double *>(buf + i + j + 2 * m);
                double *pZ = reinterpret_cast<double *>(buf + i + j + 3 * m);

                hn::Vec<decltype(d)> ure, uim, vre, vim, wre, wim, zre, zim;
                hn::LoadInterleaved2(d, pU, ure, uim);
                hn::LoadInterleaved2(d, pV, vre, vim);
                hn::LoadInterleaved2(d, pW, wre, wim);
                hn::LoadInterleaved2(d, pZ, zre, zim);

                const auto w1r = hn::LoadU(d, sW1r + j);
                const auto w1i = hn::LoadU(d, sW1i + j);
                const auto w2r = hn::LoadU(d, sW2r + j);
                const auto w2i = hn::LoadU(d, sW2i + j);
                const auto w3r = hn::LoadU(d, sW3r + j);
                const auto w3i = hn::LoadU(d, sW3i + j);

                const auto x1re = hn::NegMulAdd(vim, w1i, hn::Mul(vre, w1r));
                const auto x1im = hn::MulAdd   (vim, w1r, hn::Mul(vre, w1i));
                const auto x2re = hn::NegMulAdd(wim, w2i, hn::Mul(wre, w2r));
                const auto x2im = hn::MulAdd   (wim, w2r, hn::Mul(wre, w2i));
                const auto x3re = hn::NegMulAdd(zim, w3i, hn::Mul(zre, w3r));
                const auto x3im = hn::MulAdd   (zim, w3r, hn::Mul(zre, w3i));

                const auto are = hn::Add(ure, x2re);
                const auto aim = hn::Add(uim, x2im);
                const auto bre = hn::Sub(ure, x2re);
                const auto bim = hn::Sub(uim, x2im);
                const auto cre = hn::Add(x1re, x3re);
                const auto cim = hn::Add(x1im, x3im);
                const auto dre = hn::Sub(x1re, x3re);
                const auto dim = hn::Sub(x1im, x3im);

                hn::StoreInterleaved2(hn::Add(are, cre),
                                      hn::Add(aim, cim), d, pU);
                hn::StoreInterleaved2(hn::Sub(bre, dim),
                                      hn::Add(bim, dre), d, pV);
                hn::StoreInterleaved2(hn::Sub(are, cre),
                                      hn::Sub(aim, cim), d, pW);
                hn::StoreInterleaved2(hn::Add(bre, dim),
                                      hn::Sub(bim, dre), d, pZ);
            }
        }
        for (; j < m; ++j) {
            const Complex x0 = buf[i + j];
            const Complex x1 = buf[i + j +     m] * Complex(sW1r[j], sW1i[j]);
            const Complex x2 = buf[i + j + 2 * m] * Complex(sW2r[j], sW2i[j]);
            const Complex x3 = buf[i + j + 3 * m] * Complex(sW3r[j], sW3i[j]);
            const Complex a  = x0 + x2;
            const Complex b  = x0 - x2;
            const Complex c  = x1 + x3;
            const Complex dc = x1 - x3;
            const Complex id = Complex(-dc.imag(), dc.real());
            buf[i + j          ] = a + c;
            buf[i + j +     m  ] = b + id;
            buf[i + j + 2 * m  ] = a - c;
            buf[i + j + 3 * m  ] = b - id;
        }
    }
}

// Pure radix-4 path. Assumes N is a power of 4. The dispatcher
// must check this before calling.
void Radix4Pow4(Complex *buf, std::size_t N, const Complex *W)
{
    if (N <= 1) return;
    digitReverse4(buf, N);

    std::vector<double> sW1r(N / 4 + 4), sW1i(N / 4 + 4);
    std::vector<double> sW2r(N / 4 + 4), sW2i(N / 4 + 4);
    std::vector<double> sW3r(N / 4 + 4), sW3i(N / 4 + 4);
    const std::size_t N_half = N / 2;

    for (std::size_t len = 4; len <= N; len <<= 2) {
        radix4Stage(buf, N, len, W, N_half,
                    sW1r.data(), sW1i.data(),
                    sW2r.data(), sW2i.data(),
                    sW3r.data(), sW3i.data());
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::signal
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::signal::detail {

HWY_EXPORT(Radix4Pow4);

void fftRadix4Pow4Dispatch(Complex *buf, std::size_t N, const Complex *W)
{
    HWY_DYNAMIC_DISPATCH(Radix4Pow4)(buf, N, W);
}

} // namespace numkit::signal::detail

#endif // HWY_ONCE
