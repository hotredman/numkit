// libs/dsp/src/backends/MDspFft_simd.cpp
//
// Thin FFT dispatcher. The actual radix kernels live in their own
// translation units (MDspFft_r2_simd.cpp / MDspFft_r4_simd.cpp /
// MDspFft_r8_simd.cpp) so MSVC's per-TU inliner budget isn't shared
// across butterflies — see feedback_fft_msvc_limits memory for why
// putting them all in one TU regressed every kernel by 30-75% on
// sizes it didn't even touch.
//
// This file is a few lines of pure dispatch logic — picks the right
// kernel based on the size predicate (pow-of-4, pow-of-8, …) and
// hands off through the per-kernel dispatcher exported from each TU.

#include "FftKernels.hpp"

#include <cstddef>

namespace numkit::m::dsp::detail {

// Forward decls for the per-kernel dispatchers exported from the
// individual radix files. Each is a HWY_DYNAMIC_DISPATCH wrapper
// around the per-target function inside its respective TU.
void fftRadix2Dispatch    (Complex *buf, std::size_t N, const Complex *W);
void fftRadix4Pow4Dispatch(Complex *buf, std::size_t N, const Complex *W);

namespace {

inline std::size_t ilog2(std::size_t N)
{
    std::size_t k = 0;
    for (std::size_t n = N; n > 1; n >>= 1) ++k;
    return k;
}

inline bool isPow4(std::size_t N)
{
    if (N < 1 || (N & (N - 1)) != 0) return false;
    return (ilog2(N) % 2) == 0;
}

// Crossover between the r2 and r4 kernels.
//
// Phase-8 measurement (2026-04-22, Arrow Lake, AVX2, MSVC + Highway):
// r4 ties r2 at N=65536 (1170 µs vs 1171 µs) and slightly LOSES at
// N=262144 (6078 µs vs 5891 µs, +3%). The earlier comment claiming
// "r4 wins from 32k upward" was a hypothesis, not a measurement —
// reality on this stack is that the per-iteration savings from halving
// the stage count are eaten by the cost of 4× LoadInterleaved2 +
// 4× StoreInterleaved2 on AVX2 doubles.
//
// Threshold is kept at 1<<15 so r4 remains exercised at N=65536 etc.
// (kept correct by libs/dsp/tests Radix4PathPowerOfFourSize). Raise
// to 1u<<30 to disable r4 entirely if it ever measurably regresses;
// the dispatcher path stays in place so different toolchains/ISAs
// (clang, NEON, AVX-512) can revisit without code surgery.
constexpr std::size_t kRadix4Threshold = 1u << 15;

} // namespace

void fftRadix2Impl(Complex *buf, std::size_t N, const Complex *W)
{
    if (isPow4(N) && N >= kRadix4Threshold)
        fftRadix4Pow4Dispatch(buf, N, W);
    else
        fftRadix2Dispatch(buf, N, W);
}

} // namespace numkit::m::dsp::detail
