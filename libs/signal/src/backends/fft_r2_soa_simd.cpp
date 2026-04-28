// libs/dsp/src/backends/fft_r2_soa_simd.cpp
//
// Radix-2 FFT with Structure-of-Arrays (split real/imag) layout
// internally. The public API still takes interleaved-complex (AoS)
// buffers; this kernel converts AoS → SoA at entry, runs all stages
// on separate re/im arrays, then converts SoA → AoS at exit.
//
// Why SoA: on AVX2 doubles, the AoS kernel's LoadInterleaved2 expands
// to a vmovupd + vunpcklpd/vunpckhpd permute pair that costs roughly
// 3-4× a plain aligned load. With SoA, every load is a clean
// contiguous vmovupd — no permutes. Per butterfly:
//   AoS: 2× LoadInterleaved2  + 2× StoreInterleaved2 = 8 instructions
//        worth of permute traffic.
//   SoA: 4× LoadU + 4× StoreU = 8 plain loads/stores, no permutes.
//
// Diagnostic that motivated this: WASM SIMD128 (2 doubles per vec)
// was 2× FASTER than native AVX2 (4 doubles per vec) on the same FFT,
// because the 128-bit LoadInterleaved2 is much cheaper than the
// 256-bit one — so the wider native vectors lost more to permutes
// than they gained from doing 2× the work per instruction.
//
// AoS↔SoA conversion costs O(N) memops at start + end of the call,
// but the saving across log2(N) butterfly stages dominates.
//
// Lives in its own TU for the same MSVC inliner-budget reason as the
// r2 / r4 / Stockham split (see feedback_fft_msvc_limits memory).

#include "FftKernels.hpp"

#include <cstddef>
#include <utility>
#include <vector>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/fft_r2_soa_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::signal {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// Bit-reverse on SoA buffers — same swap pattern as the AoS kernel,
// but applied to two parallel arrays. Cache-cost is similar to AoS:
// the swap targets are scattered, but each touches one cache line per
// array (re and im), which together are roughly the same as the one
// AoS swap (both halves of the complex value live in the same line).
HWY_INLINE void bitReverse2_SoA(double *re, double *im, std::size_t N)
{
    for (std::size_t i = 1, j = 0; i < N; ++i) {
        std::size_t bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }
}

// Stages-only entry — bit-reverse + butterfly stages on already-SoA
// buffers. Used by the FFT wrapper's rfft path which packs straight
// into SoA and twists from SoA, skipping the AoS↔SoA conversion that
// the AoS-public Radix2SoA() wraps around the same code.
void Radix2SoAStages(double *re, double *im, std::size_t N, const Complex *W)
{
    if (N <= 1) return;

    bitReverse2_SoA(re, im, N);

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

                    // 4 plain aligned-but-unsafe loads — no permutes.
                    const auto u_re = hn::LoadU(d, re + i + j);
                    const auto u_im = hn::LoadU(d, im + i + j);
                    const auto v_re = hn::LoadU(d, re + i + j + half);
                    const auto v_im = hn::LoadU(d, im + i + j + half);

                    // bv = v * w (complex multiply on split real/imag):
                    //   bv_re = v_re*w_re - v_im*w_im
                    //   bv_im = v_re*w_im + v_im*w_re
                    const auto bv_re = hn::NegMulAdd(v_im, wim, hn::Mul(v_re, wre));
                    const auto bv_im = hn::MulAdd   (v_im, wre, hn::Mul(v_re, wim));

                    // Butterfly: u + bv → first half, u − bv → second half.
                    hn::StoreU(hn::Add(u_re, bv_re), d, re + i + j);
                    hn::StoreU(hn::Add(u_im, bv_im), d, im + i + j);
                    hn::StoreU(hn::Sub(u_re, bv_re), d, re + i + j + half);
                    hn::StoreU(hn::Sub(u_im, bv_im), d, im + i + j + half);
                }
            }
            for (; j < half; ++j) {
                const double u_re = re[i + j];
                const double u_im = im[i + j];
                const double v_re_in = re[i + j + half];
                const double v_im_in = im[i + j + half];
                const double bv_re = v_re_in * stageWr[j] - v_im_in * stageWi[j];
                const double bv_im = v_re_in * stageWi[j] + v_im_in * stageWr[j];
                re[i + j]        = u_re + bv_re;
                im[i + j]        = u_im + bv_im;
                re[i + j + half] = u_re - bv_re;
                im[i + j + half] = u_im - bv_im;
            }
        }
    }
}

// AoS-in/out wrapper — converts buf → re/im on entry, runs stages,
// converts re/im → buf on exit. The two conversions are O(N) and
// roughly cancel the per-stage permute saving for very small N
// (kRadix2SoaThreshold guards against that). Used by callers that
// can't yet hand SoA buffers in directly.
void Radix2SoA(Complex *buf, std::size_t N, const Complex *W,
               double *re, double *im)
{
    if (N <= 1) return;

    // AoS → SoA convert. Sequential; HW prefetcher handles it.
    for (std::size_t i = 0; i < N; ++i) {
        re[i] = buf[i].real();
        im[i] = buf[i].imag();
    }

    Radix2SoAStages(re, im, N, W);

    // SoA → AoS convert.
    for (std::size_t i = 0; i < N; ++i) {
        buf[i] = Complex(re[i], im[i]);
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::signal
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::signal::detail {

HWY_EXPORT(Radix2SoA);
HWY_EXPORT(Radix2SoAStages);

// Per-thread re/im scratch — same growth pattern as the FFT wrapper's
// working buffer in MDspFft.cpp. One persistent allocation per thread,
// grows to the largest N seen.
void fftRadix2SoaDispatch(Complex *buf, std::size_t N, const Complex *W)
{
    thread_local std::vector<double> tlsRe;
    thread_local std::vector<double> tlsIm;
    if (tlsRe.size() < N) tlsRe.resize(N);
    if (tlsIm.size() < N) tlsIm.resize(N);
    HWY_DYNAMIC_DISPATCH(Radix2SoA)(buf, N, W, tlsRe.data(), tlsIm.data());
}

// Stages-only dispatcher — caller owns re/im. Used by the FFT wrapper
// when its scratch is already in SoA layout (rfft path packs straight
// into split real/imag, then twists from there).
void fftRadix2SoaStagesDispatch(double *re, double *im, std::size_t N,
                                const Complex *W)
{
    HWY_DYNAMIC_DISPATCH(Radix2SoAStages)(re, im, N, W);
}

} // namespace numkit::signal::detail

#endif // HWY_ONCE
