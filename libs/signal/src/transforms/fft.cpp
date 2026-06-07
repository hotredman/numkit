// libs/signal/src/transforms/fft.cpp
//
// Public C++ API for 1D FFT / IFFT plus the adapters that bridge the
// Engine's MATLAB-style calling convention onto it. Algorithm is the same
// Cooley-Tukey radix-2 as before (via the fftRadix2 helper in dsp_helpers.hpp)
// — this file only restructures WHERE the logic lives (public free
// functions with an explicit memory_resource parameter vs an engine-registered
// lambda that reached into CallContext).

#include <numkit/signal/transforms/fft.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "../dsp_helpers.hpp"
#include "backends/fft_kernels.hpp"

// Highway intrinsics for the SIMD twist loop in the rfft path. Only
// the unit-stride dst case uses SIMD; non-unit stride and the scalar
// tail keep the plain C++ formulation.
#if !defined(__EMSCRIPTEN__) && defined(NUMKIT_WITH_SIMD)
  #include <hwy/highway.h>
#endif

#include <algorithm>
#include <cmath>
#include <complex>
#include <memory_resource>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace numkit::signal {

namespace {

// ── FFT scratch + twiddle cache ─────────────────────────────────────────
//
// At fftLen ≥ 32k the wrapper used to spend ~50% of its time on the
// std::pmr allocations of `buf` (fftLen complex) and `W` (fftLen/2
// complex). On Windows those translate to VirtualAlloc page commits
// (~5-10 µs per 4 KB page), and at fftLen=32k we'd commit ~190 pages
// per FFT call. WASM didn't show this cliff because its linear
// memory model has no per-page commit cost. We close the cliff by
// caching both:
//
//   * Twiddle tables — pure function of fftLen, read-only after fill,
//     safely sharable across worker threads. One process-global
//     unordered_map keyed by fftLen, mutex-guarded insert. The
//     table-vector is held by unique_ptr so the data() pointer stays
//     valid across map rehashes.
//
//   * The complex-typed working buffer — per-thread, grows monotonically.
//     Each thread reuses the same heap allocation across FFT calls;
//     resize() to fftLen is a no-op when the buffer is already large
//     enough.
//
// Trade-off: scratch memory is no longer routed through the user's
// memory_resource (so it doesn't show up in Engine accounting). Output
// Values still go through the user's memory_resource — only the
// internal scratch is process-cached. Cache memory is bounded: one entry per
// distinct power-of-two FFT size used in the program, and one per
// thread for the working buffer (sized to the largest fftLen seen).

struct TwiddleCache
{
    std::mutex mtx;
    std::unordered_map<std::size_t, std::unique_ptr<std::vector<Complex>>> tables;
};

inline TwiddleCache &twiddleCache()
{
    static TwiddleCache c;
    return c;
}

// Returns a pointer to a forward-direction twiddle table of length
// fftLen/2 for an fftLen-point FFT. Caller must not write to it.
// Inverse FFT is done via the conjugate trick in fftAlongDim, so
// only forward tables are ever cached.
//
// fillFftTwiddles convention (see dsp_helpers.hpp):
//   dir = -1  →  W[k] = exp(-2πi·k/N)  →  FORWARD DFT kernel
//   dir = +1  →  W[k] = exp(+2πi·k/N)  →  INVERSE DFT kernel
// Plugged into the standard Cooley-Tukey butterfly, the sign of the
// twiddle exponent IS the direction of the transform. We need forward
// here because the inverse path goes through conjugate-trick
// (conj → forward FFT → conj/N) in runComplex below. A pre-2026-05-10
// regression had this filled with +1 (mathematically inverse), which
// made every numkit fft return the conjugate of MATLAB's — invisible
// for real-input parity tests that check magnitudes / real parts, but
// visible for complex inputs as a spatial mirror around DC.
const Complex *getCachedTwiddleFwd(std::size_t fftLen)
{
    auto &c = twiddleCache();
    std::lock_guard<std::mutex> g(c.mtx);
    auto it = c.tables.find(fftLen);
    if (it != c.tables.end())
        return it->second->data();
    auto tbl = std::make_unique<std::vector<Complex>>(fftLen / 2);
    fillFftTwiddles(tbl->data(), fftLen, /*dir=*/-1);
    const Complex *ptr = tbl->data();
    c.tables.emplace(fftLen, std::move(tbl));
    return ptr;
}

inline bool isPow2(std::size_t n)
{
    return n != 0 && (n & (n - 1)) == 0;
}

// ── Bluestein (chirp-z) plan cache ─────────────────────────────────────
//
// For non-pow2 N we need a true N-point DFT, not a zero-padded pow2 one.
// Bluestein's identity expresses the DFT as a length-M (pow2) convolution:
//
//   X[k] = b[k] · Σₙ (x[n]·b[n]) · conj(b[k-n])     b[n] = exp(-iπn²/N)
//
// Because conj(b) is even (b[-m] = b[m]), the sequence is an even/anti-
// causal convolution kernel and the convolution can be evaluated as a
// circular conv of length M ≥ 2N-1 with the FFT of a pre-built kernel
// V[m] (V[0..N-1]=conj(b[m]); V[M-m]=conj(b[m]) for 1≤m<N; rest zero).
//
// Per-N cost:
//   - chirp[n] = exp(-iπn²/N)  for n ∈ [0, N) — one call to sin/cos per n
//   - V_freq = FFT_M(V)        — one length-M radix-2 FFT
//
// Per-call cost: 2× length-M radix-2 FFT + O(M) pointwise multiplies +
// O(N) twist. M = nextPow2(2N-1), so an N=480 FFT runs internally at
// M=1024 (vs the buggy zero-padded N=512 it used to run). ~2× the work
// of a true 480-point Cooley-Tukey, but bit-equal to MATLAB.
struct BluesteinPlan
{
    std::size_t N    = 0;          // user-visible FFT length (typically non-pow2)
    std::size_t M    = 0;          // internal convolution length, pow2 ≥ 2N-1
    std::vector<Complex> chirp;    // b[n] = exp(-iπn²/N), n ∈ [0, N)
    std::vector<Complex> V_freq;   // FFT_M of the length-M kernel
};

struct BluesteinCache
{
    std::mutex mtx;
    std::unordered_map<std::size_t, std::unique_ptr<BluesteinPlan>> plans;
};

inline BluesteinCache &bluesteinCache()
{
    static BluesteinCache c;
    return c;
}

const BluesteinPlan *getCachedBluesteinPlan(std::size_t N)
{
    auto &c = bluesteinCache();
    std::lock_guard<std::mutex> g(c.mtx);
    auto it = c.plans.find(N);
    if (it != c.plans.end())
        return it->second.get();

    auto plan = std::make_unique<BluesteinPlan>();
    plan->N = N;
    std::size_t M = 1;
    while (M < 2 * N - 1) M <<= 1;
    plan->M = M;

    // chirp[n] = exp(-iπn²/N). Reduce n²/N angle via (n² mod 2N) to keep
    // the sin/cos argument small even for moderately large N — the
    // unreduced angle πn²/N grows like π·n for n→N, costing precision
    // by N≈10⁵. n*n stays within size_t for n < 2^31.
    plan->chirp.resize(N);
    const double invN = 1.0 / static_cast<double>(N);
    for (std::size_t n = 0; n < N; ++n) {
        const std::size_t mod = (n * n) % (2 * N);
        const double angle = -M_PI * static_cast<double>(mod) * invN;
        plan->chirp[n] = Complex(std::cos(angle), std::sin(angle));
    }

    // Build kernel V[0..M-1]: V[m]=conj(b[m]) for m∈[0,N); V[M-m]=conj(b[m])
    // for m∈[1,N) (encodes b[-m]=b[m]); rest zero.
    std::vector<Complex> V(M, Complex(0.0, 0.0));
    for (std::size_t m = 0; m < N; ++m) V[m] = std::conj(plan->chirp[m]);
    for (std::size_t m = 1; m < N; ++m) V[M - m] = std::conj(plan->chirp[m]);

    // FFT(V) using the cached size-M twiddles.
    const Complex *W_M = getCachedTwiddleFwd(M);
    detail::fftRadix2Impl(V.data(), M, W_M);
    plan->V_freq = std::move(V);

    const BluesteinPlan *ptr = plan.get();
    c.plans.emplace(N, std::move(plan));
    return ptr;
}

// Per-thread reusable working buffer. Grows on first call at a new
// max size; subsequent calls at smaller sizes reuse the same
// allocation. Avoids the 0.5-1 ms VirtualAlloc page-commit cost on
// large per-call allocations.
inline std::vector<Complex> &threadFftBuf()
{
    thread_local std::vector<Complex> buf;
    return buf;
}

} // namespace

// ── Shared algorithm core ──────────────────────────────────────────────
//
// 1-D / 2-D / 3-D FFT along the specified axis (dim ∈ {1, 2, 3}).
//
// For every input layout (column-major), the algorithm is the same:
//   1. Extract the axis length + stride for the chosen dim.
//   2. Enumerate every 1-D slice along that axis (numel / axisLen slices).
//   3. For each slice: copy into a complex scratch buffer, run fftRadix2,
//      copy result back along the same stride pattern.
//
// dir = +1 for forward, -1 for inverse (conjugate-trick). ifft downgrades
// the result to real when every element's imaginary part is within 1e-10.
//
// Caller contract:
//   - dim already validated to be 1, 2, or 3 by fft()/ifft() (the
//     "first non-singleton" default — dim=0 in the public API — is
//     resolved to a concrete axis before this is called).
//   - axisLen == 1 with requested outLen > 1 throws (extending
//     dimensionality isn't supported yet).
static Value fftAlongDim(const Value &x, size_t N_req, int dim, int dir, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    const size_t R = d.rows();
    const size_t C = d.cols();
    const size_t P = d.is3D() ? d.pages() : 1;

    size_t axisLen = 0, axisStride = 0;
    switch (dim) {
    case 1: axisLen = R; axisStride = 1;       break;
    case 2: axisLen = C; axisStride = R;       break;
    case 3: axisLen = P; axisStride = R * C;   break;
    default: /* unreachable */                 break;
    }

    const size_t outAxisLen = (N_req > 0) ? N_req : axisLen;

    // Extending a singleton axis into a new dimension (e.g. dim=3 on a
    // 2-D input with N>1) would require producing a higher-rank output;
    // it's a valid MATLAB shape but falls outside the current scope.
    if (axisLen <= 1 && outAxisLen > 1)
        throw Error("fft: extending dimension beyond ndims is not supported "
                     "when the axis length is 1",
                     0, 0, "fft", "", "numkit:fft:extendDim");

    // fftLen IS the user-visible length: we compute a true N-point DFT.
    // The pow2 path uses Cooley-Tukey radix-2 directly; non-pow2 routes
    // through Bluestein's chirp-z algorithm (which internally uses a
    // pow2 FFT of length M = nextPow2(2N-1)). Previously this rounded
    // up to nextPow2(outAxisLen) and ran a zero-padded pow2 FFT, then
    // returned the first outAxisLen samples — which is NOT an N-point
    // DFT for non-pow2 N (broke conjugate symmetry, wrong magnitudes).
    const size_t fftLen   = outAxisLen;
    const size_t useLen   = std::min(axisLen, outAxisLen);
    const bool   isPow2N  = isPow2(fftLen);

    // Output shape: input shape with the chosen axis replaced.
    size_t outR = R, outC = C, outP = P;
    size_t outAxisStride = 0;
    switch (dim) {
    case 1: outR = outAxisLen; outAxisStride = 1;             break;
    case 2: outC = outAxisLen; outAxisStride = outR;          break;
    case 3: outP = outAxisLen; outAxisStride = outR * outC;   break;
    }
    const bool outIs3D = d.is3D();

    auto result = outIs3D
        ? Value::matrix3d(outR, outC, outP, ValueType::COMPLEX, mr)
        : Value::complexMatrix(outR, outC, mr);
    Complex *dst = result.complexDataMut();

    const bool srcIsComplex = x.isComplex();
    const Complex *srcC = srcIsComplex ? x.complexData() : nullptr;
    const double *srcD  = srcIsComplex ? nullptr : x.doubleData();

    // Bluestein plan (non-pow2 path only). Cached per N — first call at
    // a new N pays a one-shot O(M log M) plan build, subsequent calls
    // reuse. Pow2 sizes leave blPlan == nullptr and never touch this.
    const BluesteinPlan *blPlan = isPow2N ? nullptr
                                          : getCachedBluesteinPlan(fftLen);

    // Scratch working buffer — thread-local, grows monotonically across
    // calls. Avoids the per-call pmr/VirtualAlloc cost that was ~50% of
    // total FFT time at fftLen ≥ 32k on Windows. The caller owns the
    // workspace lifetime via the thread; clearing happens at thread exit.
    // Subsequent slices overwrite [0, useLen) fully; the tail
    // [useLen, fftLen) gets zero-filled per-slice (no-op when
    // useLen == fftLen, which is the common pow2-input case).
    //
    // The Bluestein path needs M (≥ 2N) samples instead of fftLen=N.
    const std::size_t bufSize = blPlan ? blPlan->M : fftLen;
    std::vector<Complex> &buf = threadFftBuf();
    if (buf.size() < bufSize)
        buf.resize(bufSize);

    // SoA scratch (split real/imag) for the rfft-SoA fast path on
    // native. Sized fftLen / 2 because rfft does a half-size complex
    // FFT internally. On WASM we don't take this path; the buffers
    // stay at zero size with no allocation cost.
#if !defined(__EMSCRIPTEN__) && defined(NUMKIT_WITH_SIMD)
    thread_local std::vector<double> tlsRfftRe;
    thread_local std::vector<double> tlsRfftIm;
#endif

    // Precomputed twiddle table — process-global cache keyed by FFT
    // length. The conjugate-trick handles the inverse direction, so we
    // only ever need the forward (dir=+1) twiddles. Cached pointer is
    // valid for the entire program lifetime; safe to share read-only
    // across worker threads.
    //
    // Pow2 path: table for the user-visible fftLen.
    // Bluestein path: table for the internal pow2 length M (the fftLen
    //   itself is non-pow2 and has no radix-2 twiddle table).
    const Complex *W = getCachedTwiddleFwd(blPlan ? blPlan->M : fftLen);

    // Real-input forward-FFT fast path (8e.4). Halves the work for the
    // common fft(real_vector) case by treating N real values as N/2
    // complex pairs, running an N/2-point complex FFT, then twisting.
    // Only engaged when:
    //   - forward direction (inverse doesn't benefit from this packing)
    //   - input is real (not complex)
    //   - no truncation / zero-padding (output length matches input exactly)
    //   - fftLen >= 4 (smaller is trivial; not worth a fast path)
    //   - fftLen is pow2 (the half-size FFT inside the trick is
    //     radix-2, so fftLen/2 must also be pow2; non-pow2 N goes
    //     through complex Bluestein instead)
    const bool rfftEligible = !srcIsComplex && dir == +1
                              && outAxisLen == fftLen
                              && useLen == fftLen
                              && fftLen >= 4
                              && isPow2N;
    // The half-size FFT inside rfft needs twiddles for an
    // (fftLen/2)-point FFT. That's exactly what's cached for size
    // fftLen/2 — W_half[k] = exp(+2πi·k/(fftLen/2)) = W_full(fftLen)[2k].
    // Same cache, smaller key.
    const Complex *W_half = rfftEligible ? getCachedTwiddleFwd(fftLen / 2)
                                         : nullptr;

    // Per-slice rfft. Two implementations, picked at compile time:
    //
    // * Native (AVX2): pack src directly into split real/imag scratch,
    //   call SoA-native FFT stages (no AoS conversion), twist from SoA
    //   into dst. Saves the per-call AoS↔SoA round-trip the SoA r2/r4
    //   public dispatchers would otherwise pay (~40 µs at N=16k).
    //
    // * WASM (SIMD128): pack into AoS buf (memcpy when srcStride==1),
    //   call AoS FFT, twist from AoS. SoA paths regress on this ISA
    //   because LoadInterleaved2 is cheap on 128-bit lanes — see
    //   fft_simd.cpp threshold comments.
#if !defined(__EMSCRIPTEN__) && defined(NUMKIT_WITH_SIMD)
    const std::size_t halfLen = fftLen / 2;
    if (rfftEligible) {
        if (tlsRfftRe.size() < halfLen) tlsRfftRe.resize(halfLen);
        if (tlsRfftIm.size() < halfLen) tlsRfftIm.resize(halfLen);
    }
    double *rfftRe = tlsRfftRe.data();
    double *rfftIm = tlsRfftIm.data();

    const auto runRfft = [&](const double *src, std::size_t srcStride,
                             Complex *dstSlice, std::size_t dstStride) {
        const std::size_t half = fftLen / 2;

        // Pack: deinterleave src into (re, im) half-arrays. For the
        // common stride==1 case this is just two strided reads — the
        // compiler should auto-vectorise. Strided case stays scalar.
        if (srcStride == 1) {
            for (std::size_t m = 0; m < half; ++m) {
                rfftRe[m] = src[2 * m    ];
                rfftIm[m] = src[2 * m + 1];
            }
        } else {
            for (std::size_t m = 0; m < half; ++m) {
                rfftRe[m] = src[(2 * m    ) * srcStride];
                rfftIm[m] = src[(2 * m + 1) * srcStride];
            }
        }

        // SoA-native FFT — no AoS conversion in/out.
        detail::fftSoaStagesDispatch(rfftRe, rfftIm, half, W_half);
        // rfftRe/Im now hold Z = the half-size complex FFT result.

        // DC and Nyquist (both pure real). Z[0] = (z0re + z0im, z0re - z0im).
        const double z0re = rfftRe[0], z0im = rfftIm[0];
        dstSlice[0]                = Complex(z0re + z0im, 0.0);
        dstSlice[half * dstStride] = Complex(z0re - z0im, 0.0);

        std::size_t k = 1;

        // SIMD twist for the unit-stride dst case. Processes lanes
        // values at a time: reads rfftRe/Im[k..k+lanes-1] forward,
        // reads rfftRe/Im[half-k..half-k-lanes+1] backward (Reverse
        // intrinsic), computes E/D/O/Xk vectorised, then stores both
        // dst[k..k+lanes-1] (forward) and dst[fftLen-k..fftLen-k-lanes+1]
        // (backward, with imag negated for conj). The boundary case
        // around k = half/2 stays scalar — no SIMD savings there since
        // the forward and backward windows would overlap.
        if (dstStride == 1) {
            namespace hn = hwy::HWY_NAMESPACE;
            const hn::ScalableTag<double> d;
            const std::size_t lanes = hn::Lanes(d);
            const auto vHalf = hn::Set(d, 0.5);

            // Need lanes elements ahead (k+lanes-1 <= half-1) AND lanes
            // elements behind (half-k-lanes+1 >= 1) AND windows must
            // not overlap (forward last index < backward first index):
            //   k+lanes-1 < half-k-lanes+1  ⇔  k <= (half - 2*lanes)/2
            // Simplify with a single safe upper bound:
            const std::size_t kSimdHi = (half >= 2 * lanes + 1)
                ? (half - 2 * lanes + 1) / 2 + 1
                : 0;

            for (; k + lanes <= kSimdHi; k += lanes) {
                const auto Zk_re = hn::LoadU(d, rfftRe + k);
                const auto Zk_im = hn::LoadU(d, rfftIm + k);

                // Backward read at half-k..half-k-lanes+1 → load lanes
                // starting at the lowest address (half-k-lanes+1), then
                // Reverse to put them in (half-k, half-k-1, …) order
                // matching the k-indexed forward vector.
                const auto Zj_re_raw = hn::LoadU(d, rfftRe + (half - k - lanes + 1));
                const auto Zj_im_raw = hn::LoadU(d, rfftIm + (half - k - lanes + 1));
                const auto Zj_re = hn::Reverse(d, Zj_re_raw);
                // conj(buf[half-k]) → imag part negated.
                const auto Zj_im_neg = hn::Neg(hn::Reverse(d, Zj_im_raw));

                // E = 0.5 * (Zk + conj(Zj))
                const auto E_re = hn::Mul(vHalf, hn::Add(Zk_re, Zj_re));
                const auto E_im = hn::Mul(vHalf, hn::Add(Zk_im, Zj_im_neg));
                // D = 0.5 * (Zk - conj(Zj))
                const auto D_re = hn::Mul(vHalf, hn::Sub(Zk_re, Zj_re));
                const auto D_im = hn::Mul(vHalf, hn::Sub(Zk_im, Zj_im_neg));
                // O = -i * D = (D_im, -D_re)
                const auto O_re = D_im;
                const auto O_im = hn::Neg(D_re);

                // Load W[k..k+lanes-1] in interleaved Complex form.
                // One LoadInterleaved2 here (4 ops permute) is much
                // cheaper than the alternative of pre-splitting the
                // twiddles or doing lane-by-lane scalar loads.
                hn::Vec<decltype(d)> Wk_re, Wk_im;
                const double *pW = reinterpret_cast<const double *>(W + k);
                hn::LoadInterleaved2(d, pW, Wk_re, Wk_im);

                // WO = W * O (complex multiply)
                const auto WO_re = hn::NegMulAdd(Wk_im, O_im, hn::Mul(Wk_re, O_re));
                const auto WO_im = hn::MulAdd   (Wk_im, O_re, hn::Mul(Wk_re, O_im));

                // Xk = E + WO
                const auto Xk_re = hn::Add(E_re, WO_re);
                const auto Xk_im = hn::Add(E_im, WO_im);

                // Store dst[k..k+lanes-1] = Xk (forward, AoS via interleave).
                double *pDstFwd = reinterpret_cast<double *>(dstSlice + k);
                hn::StoreInterleaved2(Xk_re, Xk_im, d, pDstFwd);

                // Store dst[fftLen-k..fftLen-k-lanes+1] = conj(Xk),
                // reversed in memory so the lowest address gets the
                // largest-k conj and addresses ascend toward dst[fftLen-k].
                const auto Xk_re_rev      = hn::Reverse(d, Xk_re);
                const auto Xk_im_rev_conj = hn::Neg(hn::Reverse(d, Xk_im));
                double *pDstBwd = reinterpret_cast<double *>(
                    dstSlice + (fftLen - k - lanes + 1));
                hn::StoreInterleaved2(Xk_re_rev, Xk_im_rev_conj, d, pDstBwd);
            }
        }

        // Scalar tail — covers non-unit stride entirely, plus the
        // middle-overlap region the SIMD body skipped.
        for (; k < half; ++k) {
            const double Zk_re  = rfftRe[k];
            const double Zk_im  = rfftIm[k];
            const double Zj_re  = rfftRe[half - k];
            const double Zj_im  = -rfftIm[half - k];   // conj(buf[half - k])
            // E = 0.5 * (Zk + conj(Zj))
            const double E_re = 0.5 * (Zk_re + Zj_re);
            const double E_im = 0.5 * (Zk_im + Zj_im);
            // D = 0.5 * (Zk - conj(Zj))
            const double D_re = 0.5 * (Zk_re - Zj_re);
            const double D_im = 0.5 * (Zk_im - Zj_im);
            // O = -i * D = (D_im, -D_re)
            const double O_re =  D_im;
            const double O_im = -D_re;
            // Xk = E + W[k] * O
            const double Wk_re = W[k].real();
            const double Wk_im = W[k].imag();
            const double WO_re = Wk_re * O_re - Wk_im * O_im;
            const double WO_im = Wk_re * O_im + Wk_im * O_re;
            const Complex Xk(E_re + WO_re, E_im + WO_im);
            dstSlice[k             * dstStride] = Xk;
            dstSlice[(fftLen - k)  * dstStride] = std::conj(Xk);
        }
    };
#else
    // WASM path — AoS throughout, with memcpy pack at srcStride==1
    // (Complex<double> is two contiguous doubles, so the deinterleave
    // pack reduces to plain memcpy).
    const auto runRfft = [&](const double *src, std::size_t srcStride,
                             Complex *dstSlice, std::size_t dstStride) {
        const std::size_t half = fftLen / 2;
        if (srcStride == 1) {
            std::memcpy(buf.data(), src, fftLen * sizeof(double));
        } else {
            for (std::size_t m = 0; m < half; ++m) {
                const double a = src[(2 * m    ) * srcStride];
                const double b = src[(2 * m + 1) * srcStride];
                buf[m] = Complex(a, b);
            }
        }
        detail::fftRadix2Impl(buf.data(), half, W_half);

        dstSlice[0]                = Complex(buf[0].real() + buf[0].imag(), 0.0);
        dstSlice[half * dstStride] = Complex(buf[0].real() - buf[0].imag(), 0.0);
        for (std::size_t k = 1; k < half; ++k) {
            const Complex Zk  = buf[k];
            const Complex Zjc = std::conj(buf[half - k]);
            const Complex E   = 0.5 * (Zk + Zjc);
            const Complex D   = 0.5 * (Zk - Zjc);
            const Complex O(D.imag(), -D.real());
            const Complex Xk  = E + W[k] * O;
            dstSlice[k             * dstStride] = Xk;
            dstSlice[(fftLen - k)  * dstStride] = std::conj(Xk);
        }
    };
#endif

    // Per-slice complex path (forward or inverse via conjugate trick).
    // Pow2-N only — non-pow2 N takes the runBluestein path below.
    const auto runComplex = [&](std::size_t inBase, std::size_t outBase,
                                std::size_t inStride, std::size_t outStride) {
        if (srcIsComplex) {
            for (std::size_t k = 0; k < useLen; ++k)
                buf[k] = srcC[inBase + k * inStride];
        } else {
            for (std::size_t k = 0; k < useLen; ++k)
                buf[k] = Complex(srcD[inBase + k * inStride], 0.0);
        }
        for (std::size_t k = useLen; k < fftLen; ++k) buf[k] = Complex(0.0, 0.0);

        if (dir == -1) {
            // Conjugate-trick over [0, fftLen) only — the thread-local
            // buf may be larger from earlier calls, so don't iterate
            // the whole vector with `for (auto &v : buf)`.
            for (std::size_t k = 0; k < fftLen; ++k) buf[k] = std::conj(buf[k]);
            detail::fftRadix2Impl(buf.data(), fftLen, W);
            const double invN = 1.0 / static_cast<double>(fftLen);
            for (std::size_t k = 0; k < fftLen; ++k) buf[k] = std::conj(buf[k]) * invN;
        } else {
            detail::fftRadix2Impl(buf.data(), fftLen, W);
        }
        for (std::size_t k = 0; k < outAxisLen; ++k)
            dst[outBase + k * outStride] = buf[k];
    };

    // Per-slice Bluestein (chirp-z) path — non-pow2 fftLen only.
    //
    // Forward direction (dir=+1):
    //   a[n]  = x[n] * b[n]                     for n ∈ [0, N)
    //   c     = IFFT_M( FFT_M(a_padded) · V_freq )
    //   X[k]  = c[k] * b[k]                     for k ∈ [0, N)
    //
    // Inverse direction (dir=-1) via conjugate trick:
    //   y[n]  = (1/N) · conj( forward_FFT( conj(x[n]) ) )
    // implemented inline by conjugating once on the way in (folded into
    // the chirp multiply) and once on the way out (folded into the final
    // conj/scale).
    const auto runBluestein = [&](std::size_t inBase, std::size_t outBase,
                                  std::size_t inStride, std::size_t outStride) {
        const std::size_t Nb = blPlan->N;
        const std::size_t M  = blPlan->M;
        const Complex *chirp = blPlan->chirp.data();
        const Complex *Vfreq = blPlan->V_freq.data();

        // Pack a[n] = x[n] * b[n] (or conj(x[n]) * b[n] for inverse,
        // complex-input case). Real input is unaffected by conj, so the
        // real path is shared.
        if (srcIsComplex && dir == -1) {
            for (std::size_t n = 0; n < useLen; ++n)
                buf[n] = std::conj(srcC[inBase + n * inStride]) * chirp[n];
        } else if (srcIsComplex) {
            for (std::size_t n = 0; n < useLen; ++n)
                buf[n] = srcC[inBase + n * inStride] * chirp[n];
        } else {
            for (std::size_t n = 0; n < useLen; ++n)
                buf[n] = Complex(srcD[inBase + n * inStride], 0.0) * chirp[n];
        }
        // Zero-pad: x[n]=0 for n in [useLen, N), AND tail [N, M).
        for (std::size_t n = useLen; n < M; ++n) buf[n] = Complex(0.0, 0.0);

        // FFT_M(a) → A.
        detail::fftRadix2Impl(buf.data(), M, W);
        // Pointwise A · V_freq.
        for (std::size_t i = 0; i < M; ++i) buf[i] *= Vfreq[i];
        // IFFT_M via conjugate trick — leaves c (length M) in buf, only
        // [0, N) is needed for the output twist.
        for (std::size_t i = 0; i < M; ++i) buf[i] = std::conj(buf[i]);
        detail::fftRadix2Impl(buf.data(), M, W);
        const double invM = 1.0 / static_cast<double>(M);
        // Twist: X[k] = (conj(c[k]) / M) * b[k].
        if (dir == +1) {
            for (std::size_t k = 0; k < Nb; ++k) {
                const Complex ck = std::conj(buf[k]) * invM;
                dst[outBase + k * outStride] = ck * chirp[k];
            }
        } else {
            // Inverse: y[k] = (1/N) · conj( X_via_forward[k] ).
            const double invN = 1.0 / static_cast<double>(Nb);
            for (std::size_t k = 0; k < Nb; ++k) {
                const Complex ck = std::conj(buf[k]) * invM;
                const Complex Xk = ck * chirp[k];
                dst[outBase + k * outStride] = std::conj(Xk) * invN;
            }
        }
    };

    // Slice enumeration. The three cases (dim=1/2/3) are spelled out
    // with concrete stride constants rather than a generic
    // lambda-with-captures — MSVC's optimiser folds the contiguous
    // (stride==1) axis-1 case into plain Load/Store sequences that way.
    const size_t numSlices = x.numel() / axisLen;
    if (dim == 1) {
        // axis = rows; inner stride is 1 — most common & tightest loop
        for (size_t s = 0; s < numSlices; ++s) {
            const size_t slicePg = s / C;
            const size_t sliceC  = s % C;
            const size_t inBase  = sliceC * R    + slicePg * R    * C;
            const size_t outBase = sliceC * outR + slicePg * outR * outC;
            if (rfftEligible)
                runRfft(srcD + inBase, /*srcStride=*/1,
                        dst    + outBase, /*dstStride=*/1);
            else if (blPlan)
                runBluestein(inBase, outBase, /*inStride=*/1, /*outStride=*/1);
            else
                runComplex(inBase, outBase, /*inStride=*/1, /*outStride=*/1);
        }
    } else {
        // dim == 2 or dim == 3 — non-unit strides, fused generic loop.
        const size_t o1Len    = (dim == 2) ? R : R;
        const size_t o1Stride = 1;
        const size_t o2Len    = (dim == 2) ? P : C;
        const size_t o2Stride = (dim == 2) ? (R * C) : R;
        const size_t o2OutStride = (dim == 2) ? (outR * outC) : outR;

        for (size_t i2 = 0; i2 < o2Len; ++i2) {
            for (size_t i1 = 0; i1 < o1Len; ++i1) {
                const size_t inBase  = i1 * o1Stride + i2 * o2Stride;
                const size_t outBase = i1 * o1Stride + i2 * o2OutStride;
                if (rfftEligible)
                    runRfft(srcD + inBase, /*srcStride=*/axisStride,
                            dst    + outBase, /*dstStride=*/outAxisStride);
                else if (blPlan)
                    runBluestein(inBase, outBase, axisStride, outAxisStride);
                else
                    runComplex(inBase, outBase, axisStride, outAxisStride);
            }
        }
    }

    // ifft: downgrade to real when every imaginary part is within
    // tolerance. Applies uniformly to 1-D / 2-D / 3-D shapes.
    if (dir == -1) {
        bool allReal = true;
        const Complex *out = result.complexData();
        for (size_t i = 0; i < result.numel() && allReal; ++i)
            if (std::abs(out[i].imag()) > 1e-10)
                allReal = false;
        if (allReal) {
            auto realOut = outIs3D
                ? Value::matrix3d(outR, outC, outP, ValueType::DOUBLE, mr)
                : Value::matrix(outR, outC, ValueType::DOUBLE, mr);
            for (size_t i = 0; i < realOut.numel(); ++i)
                realOut.doubleDataMut()[i] = result.complexData()[i].real();
            return realOut;
        }
    }

    return result;
}

// ── Public API ─────────────────────────────────────────────────────────

// Resolve dim=0 ("auto") to the first non-singleton axis, matching
// MATLAB's default for fft/ifft. Returns 1 for a pure scalar input —
// the resulting length-1 FFT is identity, so this is harmless.
static int resolveDefaultDim(const Value &x)
{
    const auto &d = x.dims();
    if (d.rows() > 1) return 1;
    if (d.cols() > 1) return 2;
    if (d.is3D() && d.pages() > 1) return 3;
    return 1;
}

Value fft(const Value &x, int n, int dim, std::pmr::memory_resource *mr)
{
    if (dim < 0 || dim > 3)
        throw Error("fft: dim must be 0 (auto), 1, 2, or 3",
                     0, 0, "fft", "", "numkit:fft:invalidDim");
    if (dim == 0) dim = resolveDefaultDim(x);

    const size_t N = (n < 0) ? 0u : static_cast<size_t>(n);
    return fftAlongDim(x, N, dim, /*dir=*/1, mr);
}

Value ifft(const Value &X, int n, int dim, std::pmr::memory_resource *mr)
{
    if (dim < 0 || dim > 3)
        throw Error("ifft: dim must be 0 (auto), 1, 2, or 3",
                     0, 0, "ifft", "", "numkit:ifft:invalidDim");
    if (dim == 0) dim = resolveDefaultDim(X);

    const size_t N = (n < 0) ? 0u : static_cast<size_t>(n);
    return fftAlongDim(X, N, dim, /*dir=*/-1, mr);
}

// ifft(X, ..., 'symmetric'): treat X as conjugate-symmetric along `dim` so
// the inverse transform is exactly real. MATLAB keeps the lower half
// X[0..floor(L/2)] authoritative — forcing the DC bin (and, for even L, the
// Nyquist bin) real — and mirrors conj(X[k]) onto X[L-k]; the upper half of
// the supplied spectrum is DISCARDED (this differs from real(ifft(X)), which
// averages the conjugate-symmetric part). Returns a real (DOUBLE) result.
// Vectors and matrices (per active dim) are supported. dim resolves like the
// regular ifft; n pads/truncates to length L before completion.
Value ifftSymmetric(const Value &X, int n, int dim, std::pmr::memory_resource *mr)
{
    const auto &d = X.dims();
    if (d.ndim() > 2)
        throw Error("ifft: the 'symmetric' option supports vectors and matrices only",
                     0, 0, "ifft", "", "numkit:ifft:symmetricNdims");
    int useDim = (dim == 0) ? resolveDefaultDim(X) : dim;
    if (useDim != 1 && useDim != 2)
        throw Error("ifft: the 'symmetric' option supports dim 1 or 2",
                     0, 0, "ifft", "", "numkit:ifft:symmetricDim");

    const std::size_t R = d.rows(), C = d.cols();
    const std::size_t origLen = (useDim == 1) ? R : C;
    const std::size_t L       = (n > 0) ? static_cast<std::size_t>(n) : origLen;

    auto getC = [&](std::size_t idx) -> Complex {
        return X.isComplex() ? X.complexData()[idx]
                             : Complex(X.elemAsDouble(idx), 0.0);
    };

    const std::size_t outR = (useDim == 1) ? L : R;
    const std::size_t outC = (useDim == 1) ? C : L;
    Value Xh = Value::matrix(outR, outC, ValueType::COMPLEX, mr);
    if (Xh.numel() == 0) return ifft(Xh, static_cast<int>(L), useDim, mr);
    Complex *h = Xh.complexDataMut();
    for (std::size_t i = 0; i < Xh.numel(); ++i) h[i] = Complex(0.0, 0.0);

    const std::size_t nslices = (useDim == 1) ? C : R;
    for (std::size_t s = 0; s < nslices; ++s) {
        auto inIdx  = [&](std::size_t j) { return (useDim == 1) ? (s * R + j) : (s + j * R); };
        auto outIdx = [&](std::size_t j) { return (useDim == 1) ? (s * outR + j) : (s + j * outR); };
        // Pad / truncate the slice to length L.
        for (std::size_t j = 0; j < L; ++j)
            h[outIdx(j)] = (j < origLen) ? getC(inIdx(j)) : Complex(0.0, 0.0);
        // Hermitian completion (lower half authoritative).
        h[outIdx(0)] = Complex(h[outIdx(0)].real(), 0.0);
        for (std::size_t k = 1; k <= (L - 1) / 2; ++k)
            h[outIdx(L - k)] = std::conj(h[outIdx(k)]);
        if (L >= 2 && (L % 2) == 0)
            h[outIdx(L / 2)] = Complex(h[outIdx(L / 2)].real(), 0.0);
    }

    Value Y = ifft(Xh, static_cast<int>(L), useDim, mr);
    // The Hermitian spectrum makes Y real to round-off; ifft already
    // auto-downgrades to DOUBLE in that case. Force real regardless so the
    // result is exactly real (MATLAB 'symmetric' never returns complex).
    if (!Y.isComplex()) return Y;
    Value re = Value::matrix(Y.dims().rows(), Y.dims().cols(), ValueType::DOUBLE, mr);
    const Complex *yc = Y.complexData();
    double *rd = re.doubleDataMut();
    for (std::size_t i = 0; i < Y.numel(); ++i) rd[i] = yc[i].real();
    return re;
}

// ifft2(X, 'symmetric'): treat X as conjugate-symmetric so the 2-D inverse
// transform is exactly real. Decomposes into the 1-D ifftSymmetric applied
// over each dimension of length > 1 (dim 2 then dim 1), matching MATLAB to
// round-off across square/non-square/row/column/scalar shapes. (The resize
// form ifft2(X,m,n,'symmetric') uses a different reconstruction and is a
// deferred gap — ifft2_reg rejects it.) 2-D only.
Value ifft2Symmetric(const Value &X, std::pmr::memory_resource *mr)
{
    const auto &d = X.dims();
    if (d.ndim() > 2)
        throw Error("ifft2: the 'symmetric' option supports 2-D inputs only",
                     0, 0, "ifft2", "", "numkit:ifft2:symmetricNdims");
    const std::size_t R = d.rows(), C = d.cols();
    Value y = X;
    if (C > 1) y = ifftSymmetric(y, -1, 2, mr);   // each row conj-symmetric
    if (R > 1) y = ifftSymmetric(y, -1, 1, mr);   // each column conj-symmetric
    if (R <= 1 && C <= 1) {
        // Scalar: real part (ifftSymmetric is never applied above).
        const Complex z = X.isComplex() ? X.complexData()[0]
                                        : Complex(X.elemAsDouble(0), 0.0);
        return Value::scalar(z.real(), mr);
    }
    // ifftSymmetric already returns a real (DOUBLE) result.
    return y;
}

// ── 2-D DFT and FFT-based interpolation (added 2026-05-03 batch 6) ───
Value fft2(const Value &X, int m, int n, std::pmr::memory_resource *mr)
{
    // fft2(X[, m, n]) = fft(fft(X, m, 1), n, 2). MATLAB pads/truncates
    // to (m, n); when omitted, uses size(X).
    Value step1 = fft(X, m, 1, mr);
    return fft(step1, n, 2, mr);
}

Value ifft2(const Value &X, int m, int n, std::pmr::memory_resource *mr)
{
    Value step1 = ifft(X, m, 1, mr);
    return ifft(step1, n, 2, mr);
}

// ── N-D FFT (added 2026-05-11) ────────────────────────────────────────
//
// fftn(X[, sz]) — apply 1-D FFT along every dimension of X. Like
// MATLAB / NumPy / SciPy this commutes, so order doesn't matter; we
// walk dims 1 → 2 → 3 because that's how the active Dims model is
// laid out (rows / cols / pages). `sz` overrides per-axis transform
// length (zero-pad or truncate before that axis's FFT). With the
// current 3-D-cap MValue, the maximum supported ndim is 3 — higher
// inputs would require the N-D refactor (planned, not yet landed).
//
// Empty input passes through with empty output (no-op).
namespace {

inline int effectiveNdim(const Value &X)
{
    const auto &d = X.dims();
    if (d.is3D()) return 3;
    if (d.cols() > 1 || d.rows() == 0) return 2;  // matrix or empty matrix
    return 2;  // 1-D row/column still counts as 2-D in our Dims model
}

Value fftnImpl(const Value &X, Span<const std::size_t> sz,
               Value (*op)(const Value &, int, int, std::pmr::memory_resource *),
               std::pmr::memory_resource *mr)
{
    if (X.isEmpty()) return X;
    const int ndim = effectiveNdim(X);
    if (sz.size() > static_cast<std::size_t>(ndim))
        throw Error("fftn: size vector length exceeds ndims(X)",
                     0, 0, "fftn", "", "numkit:fftn:badSize");
    Value Y = X;
    for (int d = 1; d <= ndim; ++d) {
        int n = -1;
        if (static_cast<std::size_t>(d) <= sz.size())
            n = static_cast<int>(sz[d - 1]);
        Y = op(Y, n, d, mr);
    }
    return Y;
}

} // anonymous namespace

Value fftn(const Value &X, Span<const std::size_t> sz,
           std::pmr::memory_resource *mr)
{
    return fftnImpl(X, sz, &fft, mr);
}

Value ifftn(const Value &X, Span<const std::size_t> sz,
            std::pmr::memory_resource *mr)
{
    return fftnImpl(X, sz, &ifft, mr);
}

// ── Chirp Z-transform (Bluestein) ─────────────────────────────────────
//
// czt(x, m, w, a) = Σ_{n=0..N-1} x[n] · a^(-n) · w^(n·k),  k=0..m-1
//
// Bluestein identity: n·k = (n² + k² − (k−n)²) / 2, so
//   Y[k] = w^(k²/2) · ((g ⋆ h)[k]) where
//     g[n] = x[n] · a^(-n) · w^(n²/2)         for n = 0..N-1
//     h[n] = w^(-n²/2)                        for n = -(N-1)..m-1
//
// Convolution evaluated via length-L FFT with L = nextPow2(N + m - 1).
// The W^(±n²/2) "chirp" terms use complex pow() — for real-w inputs
// the imaginary part comes out at fp-noise level on common parities.
//
// Single-vector kernel; matrix inputs delegate by-column above.
static Value cztVector(const Complex *xData, std::size_t N, int m, Complex w, Complex a, std::pmr::memory_resource *mr)
{
    using Cd = std::complex<double>;
    if (m <= 0) return Value::complexMatrix(0, 0, mr);
    const std::size_t M = static_cast<std::size_t>(m);
    if (N == 0) {
        // MATLAB czt of empty input → empty.
        return Value::complexMatrix(0, 0, mr);
    }

    // L = smallest power of 2 ≥ N + M - 1 (so the underlying fft hits
    // the radix-2 fast path).
    std::size_t L = 1;
    while (L < N + M - 1) L <<= 1;

    // g[n] = x[n] · a^(-n) · w^(n²/2), zero-padded to L.
    Value g = Value::complexMatrix(L, 1, mr);
    Cd *gd = g.complexDataMut();
    for (std::size_t n = 0; n < N; ++n) {
        const double n2_half = 0.5 * static_cast<double>(n) * static_cast<double>(n);
        const Cd a_negn = std::pow(a, -static_cast<double>(n));
        const Cd w_n2   = std::pow(w,  n2_half);
        gd[n] = xData[n] * a_negn * w_n2;
    }
    for (std::size_t n = N; n < L; ++n) gd[n] = Cd(0, 0);

    // h[n] = w^(-n²/2) for n ∈ {0..M-1} placed at h[0..M-1], plus the
    // negative-n branch n ∈ {1..N-1} placed at the upper end h[L-n].
    // That layout makes the circular convolution g ⊛ h (length L) equal
    // the linear convolution on the first M output samples.
    Value h = Value::complexMatrix(L, 1, mr);
    Cd *hd = h.complexDataMut();
    for (std::size_t k = 0; k < M; ++k) {
        const double k2_half = 0.5 * static_cast<double>(k) * static_cast<double>(k);
        hd[k] = std::pow(w, -k2_half);
    }
    for (std::size_t n = M; n < L; ++n) hd[n] = Cd(0, 0);
    for (std::size_t n = 1; n < N; ++n) {
        const double n2_half = 0.5 * static_cast<double>(n) * static_cast<double>(n);
        hd[L - n] = std::pow(w, -n2_half);
    }

    // FFT both, point-multiply, IFFT — standard convolution.
    Value G = fft(g, static_cast<int>(L), 1, mr);
    Value H = fft(h, static_cast<int>(L), 1, mr);
    const Cd *Gd = G.complexData();
    const Cd *Hd = H.complexData();
    Value P = Value::complexMatrix(L, 1, mr);
    Cd *Pd = P.complexDataMut();
    for (std::size_t i = 0; i < L; ++i) Pd[i] = Gd[i] * Hd[i];
    Value yL = ifft(P, static_cast<int>(L), 1, mr);

    // Take the first M samples and apply the post-multiplier
    // w^(k²/2). ifft may return real-typed when imag is ulp-clean.
    Value Y = Value::complexMatrix(1, M, mr);
    Cd *Yd = Y.complexDataMut();
    if (yL.isComplex()) {
        const Cd *yd = yL.complexData();
        for (std::size_t k = 0; k < M; ++k) {
            const double k2_half = 0.5 * static_cast<double>(k) * static_cast<double>(k);
            Yd[k] = yd[k] * std::pow(w, k2_half);
        }
    } else {
        const double *yd = yL.doubleData();
        for (std::size_t k = 0; k < M; ++k) {
            const double k2_half = 0.5 * static_cast<double>(k) * static_cast<double>(k);
            Yd[k] = Cd(yd[k], 0.0) * std::pow(w, k2_half);
        }
    }
    return Y;
}

Value czt(const Value &x, int m, Complex w, Complex a, std::pmr::memory_resource *mr)
{
    using Cd = std::complex<double>;
    if (x.isEmpty()) return Value::complexMatrix(0, 0, mr);

    const auto &d = x.dims();
    const std::size_t R = d.rows();
    const std::size_t C = d.cols();

    // Auto-orient like fft: for a row or column vector, use the length;
    // for a matrix, run czt on each column independently.
    const bool isVector = (R == 1 || C == 1) && !d.is3D();
    if (isVector) {
        const std::size_t N = x.numel();
        // Build complex source view (a private buffer when input is real).
        ScratchArena arena(mr);
        ScratchVec<Cd> buf(N, &arena);
        if (x.isComplex()) {
            const Cd *xc = x.complexData();
            for (std::size_t i = 0; i < N; ++i) buf[i] = xc[i];
        } else {
            const double *xr = x.doubleData();
            for (std::size_t i = 0; i < N; ++i) buf[i] = Cd(xr[i], 0.0);
        }
        Value y = cztVector(buf.data(), N, m, w, a, mr);
        // Match input shape (row vector → row, column → column).
        if (R == 1) return y;            // already row
        // Reshape row to column.
        Value yCol = Value::complexMatrix(static_cast<std::size_t>(m), 1, mr);
        Cd *col = yCol.complexDataMut();
        const Cd *row = y.complexData();
        for (int k = 0; k < m; ++k) col[k] = row[k];
        return yCol;
    }

    if (d.is3D())
        throw Error("czt: 3-D input not supported",
                     0, 0, "czt", "", "numkit:czt:unsupportedNd");

    // Matrix: czt each column independently.
    Value out = Value::complexMatrix(static_cast<std::size_t>(m), C, mr);
    Cd *od = out.complexDataMut();
    ScratchArena arena(mr);
    ScratchVec<Cd> col(R, &arena);
    for (std::size_t c = 0; c < C; ++c) {
        if (x.isComplex()) {
            const Cd *xc = x.complexData();
            for (std::size_t r = 0; r < R; ++r) col[r] = xc[c * R + r];
        } else {
            const double *xr = x.doubleData();
            for (std::size_t r = 0; r < R; ++r) col[r] = Cd(xr[c * R + r], 0.0);
        }
        Value y = cztVector(col.data(), R, m, w, a, mr);
        const Cd *yd = y.complexData();
        for (int k = 0; k < m; ++k) od[c * m + k] = yd[k];
    }
    return out;
}

Value interpft(const Value &x, int n, int dim, std::pmr::memory_resource *mr)
{
    if (n <= 0)
        throw Error("interpft: output length n must be positive",
                     0, 0, "interpft", "", "numkit:interpft:badN");
    if (dim < 0 || dim > 3)
        throw Error("interpft: dim must be 0 (auto), 1, 2, or 3",
                     0, 0, "interpft", "", "numkit:interpft:badDim");

    // Resolve auto-dim same way `fft` does.
    int useDim = dim;
    if (useDim == 0) {
        const auto &d = x.dims();
        if (d.rows() > 1)        useDim = 1;
        else if (d.cols() > 1)   useDim = 2;
        else if (d.is3D() && d.pages() > 1) useDim = 3;
        else                     useDim = 1;
    }

    // Length along the chosen axis.
    const auto &d = x.dims();
    size_t mLen = 1;
    if      (useDim == 1) mLen = d.rows();
    else if (useDim == 2) mLen = d.cols();
    else if (useDim == 3) mLen = d.pages();
    if (mLen == 0)
        return x;  // empty input → same shape, no-op
    if (static_cast<size_t>(n) == mLen)
        return x;  // identity case

    // FFT along that axis. fft always returns COMPLEX for non-empty input.
    Value X = fft(x, /*n=*/-1, useDim, mr);

    // Zero-pad / truncate in frequency domain. The trick: keep the
    // first half (positive frequencies) and the last half (negative
    // frequencies, mirror), drop / insert zeros in the middle.
    //
    // We implement this by calling fft with n_target on the original
    // signal — but fft's "zero-pad in time domain" is NOT the same.
    // Instead: do it manually via dim-agnostic copy.
    //
    // Easiest dim-agnostic way: do it for dim=1 and dim=2 only (numkit
    // common case), use Value indexing directly. For 3-D / dim=3 we
    // route via permutation.
    const size_t nOut = static_cast<size_t>(n);
    const size_t half = mLen / 2;        // floor(m/2)

    auto pickN = [&](size_t r, size_t c) -> Value {
        Value Y = Value::complexMatrix(r, c, mr);
        auto *dst = Y.complexDataMut();
        for (size_t i = 0; i < r * c; ++i) dst[i] = {0.0, 0.0};
        return Y;
    };

    // Scale a possibly-complex / possibly-real Value by `s` in-place.
    // ifft may have downgraded to DOUBLE when the imaginary part vanished;
    // we just multiply the underlying data either way.
    auto scaleVal = [&](Value v) -> Value {
        const double s = static_cast<double>(nOut) / static_cast<double>(mLen);
        const size_t total = v.numel();
        if (v.type() == ValueType::COMPLEX) {
            auto *p = v.complexDataMut();
            for (size_t i = 0; i < total; ++i) p[i] *= s;
        } else if (v.type() == ValueType::DOUBLE) {
            auto *p = v.doubleDataMut();
            for (size_t i = 0; i < total; ++i) p[i] *= s;
        }
        return v;
    };

    if (useDim == 1 && (d.ndim() <= 2)) {
        // Column-wise interpolation in 2-D matrix (rows × cols).
        const size_t cols = d.cols();
        Value Y = pickN(nOut, cols);
        const auto *src = X.complexData();
        auto *dst       = Y.complexDataMut();
        for (size_t c = 0; c < cols; ++c) {
            const size_t srcCol = c * mLen;
            const size_t dstCol = c * nOut;
            // Front half [0..half] → first half+1 of dst.
            for (size_t i = 0; i <= half && i < nOut; ++i)
                dst[dstCol + i] = src[srcCol + i];
            // Back half (last `half-((mLen%2==0)?1:0)` rows) → end of dst.
            const size_t backLen = mLen - half - 1;
            for (size_t i = 0; i < backLen; ++i) {
                if (nOut - backLen + i < nOut)
                    dst[dstCol + nOut - backLen + i] =
                        src[srcCol + half + 1 + i];
            }
            // Even-m: split the Nyquist bin between the two halves.
            if ((mLen & 1u) == 0u && nOut > mLen) {
                const size_t mid = half;
                dst[dstCol + mid] = src[srcCol + mid] * 0.5;
                if (nOut - mid < nOut)
                    dst[dstCol + nOut - mid] = src[srcCol + mid] * 0.5;
            }
        }
        return scaleVal(ifft(Y, /*n=*/-1, 1, mr));
    }

    if (useDim == 2 && (d.ndim() <= 2)) {
        // Row-wise interpolation. Build the padded spectrum as a (rows × nOut)
        // matrix by walking columns.
        const size_t rows = d.rows();
        Value Y = pickN(rows, nOut);
        const auto *src = X.complexData();
        auto *dst       = Y.complexDataMut();
        const size_t backLen = mLen - half - 1;
        for (size_t r = 0; r < rows; ++r) {
            for (size_t i = 0; i <= half && i < nOut; ++i)
                dst[i * rows + r] = src[i * rows + r];
            for (size_t i = 0; i < backLen; ++i) {
                if (nOut - backLen + i < nOut)
                    dst[(nOut - backLen + i) * rows + r] =
                        src[(half + 1 + i) * rows + r];
            }
            if ((mLen & 1u) == 0u && nOut > mLen) {
                const size_t mid = half;
                dst[mid * rows + r] = src[mid * rows + r] * 0.5;
                if (nOut - mid < nOut)
                    dst[(nOut - mid) * rows + r] = src[mid * rows + r] * 0.5;
            }
        }
        return scaleVal(ifft(Y, /*n=*/-1, 2, mr));
    }

    throw Error("interpft: dim 3 / N-D inputs not yet supported",
                 0, 0, "interpft", "", "numkit:interpft:dimUnsupported");
}

} // namespace numkit::signal
