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
void fftStockhamDispatch  (Complex *buf, std::size_t N, const Complex *W);
void fftRadix2SoaDispatch (Complex *buf, std::size_t N, const Complex *W);

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

// Stockham auto-sort kernel: out-of-place radix-2 that skips the
// bit-reversal pre-pass. Costs 2× working memory (one extra N-complex
// scratch held in a thread-local cache).
//
// Currently DISABLED on this stack — the bit-reversal cost it saves
// (~35% of r2 kernel time at N=32k) is more than offset by the cold-
// buffer write penalty Stockham incurs writing to a fresh scratch
// instead of an already-L1-resident in-place buffer. Diagnostic
// micro-benches at N=32k native AVX2/MSVC:
//   r2 kernel total       : 282 µs
//   bit-reverse alone     :  99 µs (35%)
//   r2 stages alone       : 183 µs
//   Stockham total        : 327 µs  (16% slower than r2!)
//   Stockham "stages"     : ~320 µs (vs r2 stages 183 µs — write-allocate
//                                     to the cold dst buffer dominates)
//
// Code + parity tests kept as documented exploration: revisit if/when
// we move to SoA layout (where write-allocate cost may amortise) or
// run on different ISAs (NEON, AVX-512). Threshold set to never-fire
// so the dispatcher always reaches r2 / r4.
constexpr std::size_t kStockhamThreshold = std::size_t(-1);

// SoA r2 kernel: split-real/imag layout that eliminates the AVX2
// LoadInterleaved2/StoreInterleaved2 permute cost. At N=2k–32k on
// AVX2 it beats the AoS r2 kernel by 20–34%; at N=1024 the AoS↔SoA
// conversion overhead costs more than the per-stage savings, so AoS
// stays as the default for tiny N.
//
// Disabled on WASM (Emscripten target): WASM SIMD128 doubles have
// only 2 lanes, so LoadInterleaved2 reduces to a cheap unpcklpd /
// unpckhpd pair. The SoA path's conversion overhead doesn't pay
// back on this ISA; bench at user N=32k showed a ~5-7% regression.
// The 256-bit AVX2 LoadInterleaved2 is what's expensive, and only
// native builds hit that path.
#if defined(__EMSCRIPTEN__)
constexpr std::size_t kRadix2SoaThreshold = std::size_t(-1);
#else
constexpr std::size_t kRadix2SoaThreshold = 1u << 11;
#endif

} // namespace

void fftRadix2Impl(Complex *buf, std::size_t N, const Complex *W)
{
    if (isPow4(N) && N >= kRadix4Threshold)
        fftRadix4Pow4Dispatch(buf, N, W);
    else if (N >= kStockhamThreshold)
        fftStockhamDispatch(buf, N, W);
    else if (N >= kRadix2SoaThreshold)
        fftRadix2SoaDispatch(buf, N, W);
    else
        fftRadix2Dispatch(buf, N, W);
}

} // namespace numkit::m::dsp::detail
