// libs/dsp/src/backends/MDspFft_simd.cpp
//
// Highway dynamic-dispatch FFT kernel. Two dispatched code paths:
//
//   N = 4^k   → base-4 digit-reversal + radix-4 stages (half the
//               number of passes, ~15 % fewer flops vs radix-2, plus
//               denser SIMD utilisation).
//   otherwise → base-2 bit-reversal + radix-2 stages (same kernel as
//               Phase 8e.2).
//
// The radix-4 butterfly in its natural form reads inputs in
// base-4 digit-reversed order and writes outputs in natural order.
// Mixing it with base-2 bit-reversal would require per-stage swap
// corrections; for the 8e.3 scope we only take the r4 path when N
// is a clean power of 4 (every bench size RangeMultiplier(4)
// satisfies this).
//
// Twiddle access: per-stage SoA scratch arrays filled once per stage
// so the inner SIMD body uses plain contiguous hn::Load — no gather.
// Complex multiplies are SoA-form FMAs, no shuffles.

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

// ── log2(N) for power-of-two N; 0 for 0/1 ────────────────────────────
HWY_INLINE std::size_t ilog2(std::size_t N)
{
    std::size_t k = 0;
    for (std::size_t n = N; n > 1; n >>= 1) ++k;
    return k;
}

// ── Predicate: N is a clean power of 4 (implies also a power of 2) ───
HWY_INLINE bool isPow4(std::size_t N)
{
    if (N < 1 || (N & (N - 1)) != 0) return false;
    return (ilog2(N) % 2) == 0;
}

// ── Base-4 digit-reversal permutation (in-place) ─────────────────────
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

// ── Base-2 bit-reversal (for the r2 fallback path) ───────────────────
HWY_INLINE void bitReverse2(Complex *buf, std::size_t N)
{
    for (std::size_t i = 1, j = 0; i < N; ++i) {
        std::size_t bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(buf[i], buf[j]);
    }
}

// ── Radix-4 stage (assumes base-4 digit-reversed input layout) ───────
//
// At stage len (= 4·m), m = len/4, step = N/len, for each group of
// `len` elements starting at i and each j in [0, m):
//
//     x0 = buf[i + j       ]  (twiddle 1)
//     x1 = buf[i + j +   m ] * W^{  j * step}
//     x2 = buf[i + j + 2·m ] * W^{2·j * step}
//     x3 = buf[i + j + 3·m ] * W^{3·j * step}
//
//     a = x0 + x2        b = x0 - x2
//     c = x1 + x3        d = x1 - x3
//     id = i·d  (complex multiply by i: (-d.im, d.re))
//
//     buf[i + j       ] = a + c
//     buf[i + j +   m ] = b + id
//     buf[i + j + 2·m ] = a - c
//     buf[i + j + 3·m ] = b - id
HWY_NOINLINE void radix4Stage(Complex *buf, std::size_t N, std::size_t len,
                            const Complex *W, std::size_t N_half,
                            double *sW1r, double *sW1i,
                            double *sW2r, double *sW2i,
                            double *sW3r, double *sW3i)
{
    const hn::ScalableTag<double> d;
    const std::size_t lanes = hn::Lanes(d);
    const std::size_t m = len / 4;
    const std::size_t step = N / len;

    // W1 and W2 indices stay within [0, N/2) for every stage so we
    // can index W directly. Only W3 (up to 3·(m-1)·step ≈ 3N/4) can
    // land in [N/2, N) and needs the W[k+N/2] = -W[k] sign flip.
    for (std::size_t k = 0; k < m; ++k) {
        const Complex w1 = W[k * step];
        const Complex w2 = W[2 * k * step];
        const std::size_t i3 = 3 * k * step;
        const Complex w3 = (i3 < N_half) ? W[i3]
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

                // x1 = v·W1, x2 = w·W2, x3 = z·W3 (SoA complex mul via FMA)
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
                // id = (-dim, dre)

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

        // Scalar tail / small-stage fallback (m < lanes, e.g. the
        // first r4 stage at len=4 where m=1).
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

void FftRadix2(Complex *buf, std::size_t N, const Complex *W)
{
    if (N <= 1) return;

    // Radix-4 wins structurally (fewer passes, fewer flops, better
    // SIMD utilisation) but carries extra per-stage overhead from
    // LoadInterleaved2/StoreInterleaved2 x4 and from computing 3
    // twiddles per butterfly vs 1. Measurements on AVX2 show r4 only
    // starts winning around N ≥ 64k; below that the r2 kernel is
    // either faster or tied. Threshold chosen empirically from the
    // BM_Fft_PowerOfTwo benchmark crossover.
    constexpr std::size_t kRadix4Threshold = 1u << 15;  // 32768

    if (isPow4(N) && N >= kRadix4Threshold) {
        // ── Pure radix-4 path ─────────────────────────────────────────
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
    } else {
        // ── Radix-2 fallback (Phase 8e.2 kernel, fully inlined) ──────
        bitReverse2(buf, N);

        std::vector<double> stageWr(N / 2), stageWi(N / 2);
        const hn::ScalableTag<double> d;
        const std::size_t lanes = hn::Lanes(d);

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
