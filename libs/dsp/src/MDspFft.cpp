// libs/dsp/src/MDspFft.cpp
//
// Public C++ API for 1D FFT / IFFT plus the adapters that bridge the
// Engine's MATLAB-style calling convention onto it. Algorithm is the same
// Cooley-Tukey radix-2 as before (via the fftRadix2 helper in MDspHelpers)
// — this file only restructures WHERE the logic lives (public free
// functions with an explicit Allocator parameter vs an engine-registered
// lambda that reached into CallContext).

#include <numkit/m/dsp/MDspFft.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MDspHelpers.hpp"
#include "backends/FftKernels.hpp"

// Highway intrinsics for the SIMD twist loop in the rfft path. Only
// the unit-stride dst case uses SIMD; non-unit stride and the scalar
// tail keep the plain C++ formulation.
#if !defined(__EMSCRIPTEN__)
  #include <hwy/highway.h>
#endif

#include <algorithm>
#include <cmath>
#include <complex>
#include <memory_resource>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace numkit::m::dsp {

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
// Allocator (so it doesn't show up in Engine accounting). Output
// MValues still go through the user's Allocator — only the internal
// scratch is process-cached. Cache memory is bounded: one entry per
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
const Complex *getCachedTwiddleFwd(std::size_t fftLen)
{
    auto &c = twiddleCache();
    std::lock_guard<std::mutex> g(c.mtx);
    auto it = c.tables.find(fftLen);
    if (it != c.tables.end())
        return it->second->data();
    auto tbl = std::make_unique<std::vector<Complex>>(fftLen / 2);
    fillFftTwiddles(tbl->data(), fftLen, /*dir=*/+1);
    const Complex *ptr = tbl->data();
    c.tables.emplace(fftLen, std::move(tbl));
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
static MValue fftAlongDim(const MValue &x, size_t N_req, int dim, int dir, Allocator *alloc)
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
        throw MError("fft: extending dimension beyond ndims is not supported "
                     "when the axis length is 1",
                     0, 0, "fft", "", "m:fft:extendDim");

    const size_t fftLen = nextPow2(outAxisLen);
    const size_t useLen = std::min(axisLen, outAxisLen);

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
        ? MValue::matrix3d(outR, outC, outP, MType::COMPLEX, alloc)
        : MValue::complexMatrix(outR, outC, alloc);
    Complex *dst = result.complexDataMut();

    const bool srcIsComplex = x.isComplex();
    const Complex *srcC = srcIsComplex ? x.complexData() : nullptr;
    const double *srcD  = srcIsComplex ? nullptr : x.doubleData();

    // Scratch working buffer — thread-local, grows monotonically across
    // calls. Avoids the per-call pmr/VirtualAlloc cost that was ~50% of
    // total FFT time at fftLen ≥ 32k on Windows. The caller owns the
    // workspace lifetime via the thread; clearing happens at thread exit.
    // Subsequent slices overwrite [0, useLen) fully; the tail
    // [useLen, fftLen) gets zero-filled per-slice (no-op when
    // useLen == fftLen, which is the common pow2-input case).
    std::vector<Complex> &buf = threadFftBuf();
    if (buf.size() < fftLen)
        buf.resize(fftLen);

    // SoA scratch (split real/imag) for the rfft-SoA fast path on
    // native. Sized fftLen / 2 because rfft does a half-size complex
    // FFT internally. On WASM we don't take this path; the buffers
    // stay at zero size with no allocation cost.
#if !defined(__EMSCRIPTEN__)
    thread_local std::vector<double> tlsRfftRe;
    thread_local std::vector<double> tlsRfftIm;
#endif

    // Precomputed twiddle table — process-global cache keyed by fftLen.
    // The conjugate-trick handles the inverse direction, so we only ever
    // need the forward (dir=+1) twiddles. Cached pointer is valid for
    // the entire program lifetime; safe to share read-only across worker
    // threads.
    const Complex *W = getCachedTwiddleFwd(fftLen);

    // Real-input forward-FFT fast path (8e.4). Halves the work for the
    // common fft(real_vector) case by treating N real values as N/2
    // complex pairs, running an N/2-point complex FFT, then twisting.
    // Only engaged when:
    //   - forward direction (inverse doesn't benefit from this packing)
    //   - input is real (not complex)
    //   - no truncation / zero-padding (output length matches input exactly)
    //   - fftLen >= 4 (smaller is trivial; not worth a fast path)
    const bool rfftEligible = !srcIsComplex && dir == +1
                              && outAxisLen == fftLen
                              && useLen == fftLen
                              && fftLen >= 4;
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
    //   MDspFft_simd.cpp threshold comments.
#if !defined(__EMSCRIPTEN__)
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
                ? MValue::matrix3d(outR, outC, outP, MType::DOUBLE, alloc)
                : MValue::matrix(outR, outC, MType::DOUBLE, alloc);
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
static int resolveDefaultDim(const MValue &x)
{
    const auto &d = x.dims();
    if (d.rows() > 1) return 1;
    if (d.cols() > 1) return 2;
    if (d.is3D() && d.pages() > 1) return 3;
    return 1;
}

MValue fft(Allocator &alloc, const MValue &x, int n, int dim)
{
    if (dim < 0 || dim > 3)
        throw MError("fft: dim must be 0 (auto), 1, 2, or 3",
                     0, 0, "fft", "", "m:fft:invalidDim");
    if (dim == 0) dim = resolveDefaultDim(x);

    const size_t N = (n < 0) ? 0u : static_cast<size_t>(n);
    return fftAlongDim(x, N, dim, /*dir=*/1, &alloc);
}

MValue ifft(Allocator &alloc, const MValue &X, int n, int dim)
{
    if (dim < 0 || dim > 3)
        throw MError("ifft: dim must be 0 (auto), 1, 2, or 3",
                     0, 0, "ifft", "", "m:ifft:invalidDim");
    if (dim == 0) dim = resolveDefaultDim(X);

    const size_t N = (n < 0) ? 0u : static_cast<size_t>(n);
    return fftAlongDim(X, N, dim, /*dir=*/-1, &alloc);
}

// ── Engine adapters ────────────────────────────────────────────────────
//
// Marshal MATLAB calling convention (variable nargin, MValue args)
// onto the explicit-alloc public API above. Throws from the public API
// propagate up — TreeWalker / VM catch blocks attach source location via
// MError::attachIfMissing.

namespace detail {

void fft_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("fft: requires at least 1 argument",
                     0, 0, "fft", "", "m:fft:nargin");

    int n = -1;
    int dim = 0;   // 0 = auto (first non-singleton) — resolved in public fft()
    if (args.size() >= 2 && !args[1].isEmpty())
        n = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3)
        dim = static_cast<int>(args[2].toScalar());

    outs[0] = fft(ctx.engine->allocator(), args[0], n, dim);
}

void ifft_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("ifft: requires at least 1 argument",
                     0, 0, "ifft", "", "m:ifft:nargin");

    int n = -1;
    int dim = 0;   // 0 = auto (first non-singleton) — resolved in public fft()
    if (args.size() >= 2 && !args[1].isEmpty())
        n = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3)
        dim = static_cast<int>(args[2].toScalar());

    outs[0] = ifft(ctx.engine->allocator(), args[0], n, dim);
}

} // namespace detail

} // namespace numkit::m::dsp
