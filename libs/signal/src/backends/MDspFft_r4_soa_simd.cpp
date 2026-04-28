// libs/dsp/src/backends/MDspFft_r4_soa_simd.cpp
//
// Radix-4 FFT for power-of-4 sizes with SoA (split real/imag) layout.
// Same algorithm as MDspFft_r4_simd.cpp; the difference is internal
// data layout — re[] and im[] kept as separate aligned arrays so
// every load is a clean vmovupd, no LoadInterleaved2 permute.
//
// On AVX2 doubles the AoS r4 paid 4× LoadInterleaved2 + 4× StoreInterleaved2
// per inner iteration — 8 permute-heavy ops which dominated the kernel
// at the size where r4 should have started winning over r2 (>= 32k).
// SoA replaces them with 8 plain LoadU + 8 plain StoreU, no permutes.
//
// Same per-TU isolation rationale as the other FFT kernels (MSVC
// inliner-budget cliff documented in feedback_fft_msvc_limits memory).

#include "FftKernels.hpp"

#include <cstddef>
#include <utility>
#include <vector>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/MDspFft_r4_soa_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::m::signal {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

HWY_INLINE std::size_t ilog2_r4(std::size_t N)
{
    std::size_t k = 0;
    for (std::size_t n = N; n > 1; n >>= 1) ++k;
    return k;
}

// Base-4 digit-reversal on SoA: same swap pattern as the AoS version,
// applied in parallel to the re[] and im[] arrays.
HWY_INLINE void digitReverse4_SoA(double *re, double *im, std::size_t N)
{
    const std::size_t k = ilog2_r4(N) / 2;
    for (std::size_t i = 0; i < N; ++i) {
        std::size_t j = 0;
        std::size_t x = i;
        for (std::size_t d = 0; d < k; ++d) {
            j = (j << 2) | (x & 3);
            x >>= 2;
        }
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }
}

// Radix-4 SoA stage. Same butterfly math as the AoS version (a/b/c/d
// derivations match), but with SoA loads/stores instead of
// LoadInterleaved2/StoreInterleaved2.
HWY_NOINLINE void radix4Stage_SoA(double *re, double *im,
                                  std::size_t N, std::size_t len,
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
                // 4 pairs (re/im) of plain loads — no permutes.
                const auto u_re = hn::LoadU(d, re + i + j);
                const auto u_im = hn::LoadU(d, im + i + j);
                const auto v_re = hn::LoadU(d, re + i + j +     m);
                const auto v_im = hn::LoadU(d, im + i + j +     m);
                const auto w_re_in = hn::LoadU(d, re + i + j + 2 * m);
                const auto w_im_in = hn::LoadU(d, im + i + j + 2 * m);
                const auto z_re = hn::LoadU(d, re + i + j + 3 * m);
                const auto z_im = hn::LoadU(d, im + i + j + 3 * m);

                const auto w1r = hn::LoadU(d, sW1r + j);
                const auto w1i = hn::LoadU(d, sW1i + j);
                const auto w2r = hn::LoadU(d, sW2r + j);
                const auto w2i = hn::LoadU(d, sW2i + j);
                const auto w3r = hn::LoadU(d, sW3r + j);
                const auto w3i = hn::LoadU(d, sW3i + j);

                // Twiddle multiplies: x_k = (v/w/z) * (w1/w2/w3).
                const auto x1re = hn::NegMulAdd(v_im, w1i, hn::Mul(v_re, w1r));
                const auto x1im = hn::MulAdd   (v_im, w1r, hn::Mul(v_re, w1i));
                const auto x2re = hn::NegMulAdd(w_im_in, w2i, hn::Mul(w_re_in, w2r));
                const auto x2im = hn::MulAdd   (w_im_in, w2r, hn::Mul(w_re_in, w2i));
                const auto x3re = hn::NegMulAdd(z_im, w3i, hn::Mul(z_re, w3r));
                const auto x3im = hn::MulAdd   (z_im, w3r, hn::Mul(z_re, w3i));

                // Radix-4 butterfly: a = u + x2, b = u - x2,
                //                    c = x1 + x3, d_pair = x1 - x3.
                const auto are = hn::Add(u_re, x2re);
                const auto aim = hn::Add(u_im, x2im);
                const auto bre = hn::Sub(u_re, x2re);
                const auto bim = hn::Sub(u_im, x2im);
                const auto cre = hn::Add(x1re, x3re);
                const auto cim = hn::Add(x1im, x3im);
                const auto dre = hn::Sub(x1re, x3re);
                const auto dim = hn::Sub(x1im, x3im);

                // Outputs (matching AoS version):
                //   pos 0: a + c
                //   pos m: b - i*dc  → real: b - dim·(-1), imag: b + dre
                //                       == (bre - dim,  bim + dre)
                //   pos 2m: a - c
                //   pos 3m: b + i*dc → (bre + dim, bim - dre)
                hn::StoreU(hn::Add(are, cre), d, re + i + j);
                hn::StoreU(hn::Add(aim, cim), d, im + i + j);
                hn::StoreU(hn::Sub(bre, dim), d, re + i + j +     m);
                hn::StoreU(hn::Add(bim, dre), d, im + i + j +     m);
                hn::StoreU(hn::Sub(are, cre), d, re + i + j + 2 * m);
                hn::StoreU(hn::Sub(aim, cim), d, im + i + j + 2 * m);
                hn::StoreU(hn::Add(bre, dim), d, re + i + j + 3 * m);
                hn::StoreU(hn::Sub(bim, dre), d, im + i + j + 3 * m);
            }
        }
        for (; j < m; ++j) {
            const double u_re = re[i + j];
            const double u_im = im[i + j];
            const double v_re = re[i + j +     m];
            const double v_im = im[i + j +     m];
            const double w_re = re[i + j + 2 * m];
            const double w_im = im[i + j + 2 * m];
            const double z_re = re[i + j + 3 * m];
            const double z_im = im[i + j + 3 * m];

            const double x1re = v_re * sW1r[j] - v_im * sW1i[j];
            const double x1im = v_re * sW1i[j] + v_im * sW1r[j];
            const double x2re = w_re * sW2r[j] - w_im * sW2i[j];
            const double x2im = w_re * sW2i[j] + w_im * sW2r[j];
            const double x3re = z_re * sW3r[j] - z_im * sW3i[j];
            const double x3im = z_re * sW3i[j] + z_im * sW3r[j];

            const double are = u_re + x2re, aim = u_im + x2im;
            const double bre = u_re - x2re, bim = u_im - x2im;
            const double cre = x1re + x3re, cim = x1im + x3im;
            const double dre = x1re - x3re, dim = x1im - x3im;

            re[i + j]            = are + cre;  im[i + j]            = aim + cim;
            re[i + j +     m]    = bre - dim;  im[i + j +     m]    = bim + dre;
            re[i + j + 2 * m]    = are - cre;  im[i + j + 2 * m]    = aim - cim;
            re[i + j + 3 * m]    = bre + dim;  im[i + j + 3 * m]    = bim - dre;
        }
    }
}

// Stages-only entry — bit-reverse + butterfly stages on already-SoA
// buffers. Skips the AoS↔SoA conversion that the AoS-public
// Radix4Pow4_SoA() wraps around the same code.
void Radix4Pow4_SoAStages(double *re, double *im, std::size_t N, const Complex *W)
{
    if (N <= 1) return;

    digitReverse4_SoA(re, im, N);

    std::vector<double> sW1r(N / 4 + 4), sW1i(N / 4 + 4);
    std::vector<double> sW2r(N / 4 + 4), sW2i(N / 4 + 4);
    std::vector<double> sW3r(N / 4 + 4), sW3i(N / 4 + 4);
    const std::size_t N_half = N / 2;

    for (std::size_t len = 4; len <= N; len <<= 2) {
        radix4Stage_SoA(re, im, N, len, W, N_half,
                        sW1r.data(), sW1i.data(),
                        sW2r.data(), sW2i.data(),
                        sW3r.data(), sW3i.data());
    }
}

// AoS-in/out wrapper — converts buf → re/im, runs stages, converts back.
void Radix4Pow4_SoA(Complex *buf, std::size_t N, const Complex *W,
                    double *re, double *im)
{
    if (N <= 1) return;

    // AoS → SoA convert.
    for (std::size_t i = 0; i < N; ++i) {
        re[i] = buf[i].real();
        im[i] = buf[i].imag();
    }

    Radix4Pow4_SoAStages(re, im, N, W);

    // SoA → AoS convert.
    for (std::size_t i = 0; i < N; ++i) {
        buf[i] = Complex(re[i], im[i]);
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::m::signal
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::m::signal::detail {

HWY_EXPORT(Radix4Pow4_SoA);
HWY_EXPORT(Radix4Pow4_SoAStages);

// Same thread-local re/im scratch pattern as the SoA r2 dispatcher.
void fftRadix4Pow4SoaDispatch(Complex *buf, std::size_t N, const Complex *W)
{
    thread_local std::vector<double> tlsRe;
    thread_local std::vector<double> tlsIm;
    if (tlsRe.size() < N) tlsRe.resize(N);
    if (tlsIm.size() < N) tlsIm.resize(N);
    HWY_DYNAMIC_DISPATCH(Radix4Pow4_SoA)(buf, N, W, tlsRe.data(), tlsIm.data());
}

// Stages-only dispatcher — caller owns re/im. Used by the wrapper
// when its scratch is already in SoA layout.
void fftRadix4Pow4SoaStagesDispatch(double *re, double *im, std::size_t N,
                                    const Complex *W)
{
    HWY_DYNAMIC_DISPATCH(Radix4Pow4_SoAStages)(re, im, N, W);
}

} // namespace numkit::m::signal::detail

#endif // HWY_ONCE
