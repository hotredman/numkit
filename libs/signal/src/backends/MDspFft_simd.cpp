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

namespace numkit::m::signal::detail {

// Forward decls for the per-kernel dispatchers exported from the
// individual radix files. Each is a HWY_DYNAMIC_DISPATCH wrapper
// around the per-target function inside its respective TU.
void fftRadix2Dispatch              (Complex *buf, std::size_t N, const Complex *W);
void fftRadix4Pow4Dispatch          (Complex *buf, std::size_t N, const Complex *W);
void fftStockhamDispatch            (Complex *buf, std::size_t N, const Complex *W);
void fftRadix2SoaDispatch           (Complex *buf, std::size_t N, const Complex *W);
void fftRadix4Pow4SoaDispatch       (Complex *buf, std::size_t N, const Complex *W);
void fftRadix2SoaStagesDispatch     (double *re, double *im, std::size_t N, const Complex *W);
void fftRadix4Pow4SoaStagesDispatch (double *re, double *im, std::size_t N, const Complex *W);

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

// AoS r4: superseded by SoA r4 on every size where it used to win
// (and tied or lost on the rest), so the AoS r4 path is now off by
// default. Kept built (correct + tested by Radix4PathPowerOfFourSize)
// so future ISAs/toolchains can re-enable without code surgery.
//
// History: r4 ties r2 at N=65536 (1170 µs vs 1171 µs) and slightly
// LOSES at N=262144 (6078 µs vs 5891 µs, +3%) on AoS — the per-stage
// savings from halving the stage count are eaten by the cost of
// 4× LoadInterleaved2 + 4× StoreInterleaved2 on AVX2 doubles. SoA
// removes those permutes, which finally lets r4 deliver its expected
// algorithmic win.
constexpr std::size_t kRadix4Threshold = std::size_t(-1);

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

// SoA r2 / SoA r4: split-real/imag layout that eliminates the AVX2
// LoadInterleaved2/StoreInterleaved2 permute cost. SoA r2 wins
// 20-34% over AoS r2 at N=2k-32k. SoA r4 wins another 12-21% on
// pow-of-4 sizes >= 16k by halving the stage count.
//
// Disabled on WASM (Emscripten target): WASM SIMD128 doubles have
// only 2 lanes, so LoadInterleaved2 reduces to a cheap unpcklpd /
// unpckhpd pair. The SoA paths' conversion overhead doesn't pay back
// on this ISA; bench at user N=32k showed a ~5-7% regression. The
// 256-bit AVX2 LoadInterleaved2 is what's expensive, and only native
// builds hit that path.
#if defined(__EMSCRIPTEN__)
constexpr std::size_t kRadix2SoaThreshold    = std::size_t(-1);
constexpr std::size_t kRadix4SoaThreshold    = std::size_t(-1);
#else
constexpr std::size_t kRadix2SoaThreshold    = 1u << 11;  // 2048
constexpr std::size_t kRadix4SoaThreshold    = 1u << 14;  // 16384
#endif

} // namespace

void fftRadix2Impl(Complex *buf, std::size_t N, const Complex *W)
{
    // SoA r4 first — best on pow-of-4 sizes where it applies.
    if (isPow4(N) && N >= kRadix4SoaThreshold)
        fftRadix4Pow4SoaDispatch(buf, N, W);
    // Legacy AoS r4 — currently disabled (kRadix4Threshold = ∞).
    else if (isPow4(N) && N >= kRadix4Threshold)
        fftRadix4Pow4Dispatch(buf, N, W);
    // Stockham — currently disabled (write-allocate cost dominates).
    else if (N >= kStockhamThreshold)
        fftStockhamDispatch(buf, N, W);
    // SoA r2 — wins on AVX2 at N >= 2k.
    else if (N >= kRadix2SoaThreshold)
        fftRadix2SoaDispatch(buf, N, W);
    // AoS r2 — fallback for tiny N and WASM.
    else
        fftRadix2Dispatch(buf, N, W);
}

void fftSoaStagesDispatch(double *re, double *im, std::size_t N, const Complex *W)
{
#if defined(__EMSCRIPTEN__)
    // SoA paths are off on WASM (SIMD128's LoadInterleaved2 is cheap).
    // The wrapper's rfft-SoA path doesn't get called there, but if it
    // did, we'd need to materialise to AoS, run the AoS kernel, and
    // split back. Ifdef'd to never compile that branch on native.
    (void)re; (void)im; (void)N; (void)W;
#else
    if (isPow4(N) && N >= kRadix4SoaThreshold)
        fftRadix4Pow4SoaStagesDispatch(re, im, N, W);
    else
        fftRadix2SoaStagesDispatch(re, im, N, W);
#endif
}

} // namespace numkit::m::signal::detail
