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

#include <algorithm>
#include <cmath>
#include <complex>
#include <memory_resource>
#include <vector>

namespace numkit::m::dsp {

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

    // buf is default-constructed zero; subsequent slices overwrite
    // [0, useLen) fully and only need the tail [useLen, fftLen) zeroed.
    // For the common power-of-two case useLen == fftLen and this costs
    // nothing; with zero-padding the reset is O(fftLen - useLen).
    //
    // Backed by the Engine's pmr-bridged allocator so scratch buffers
    // are accounted for through the same tracked path as MValue heap
    // storage — no silent std::allocator-backed allocations here.
    std::pmr::vector<Complex> buf(fftLen, alloc->memoryResource());

    // Precomputed twiddle table — size N/2, reused across every slice.
    // The conjugate-trick below handles the inverse direction, so
    // fftRadix2 is always called with forward (dir=+1) twiddles.
    std::pmr::vector<Complex> W(fftLen / 2, alloc->memoryResource());
    fillFftTwiddles(W.data(), fftLen, /*dir=*/+1);

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
            // Load contiguous
            if (srcIsComplex) {
                for (size_t k = 0; k < useLen; ++k) buf[k] = srcC[inBase + k];
            } else {
                for (size_t k = 0; k < useLen; ++k) buf[k] = Complex(srcD[inBase + k], 0.0);
            }
            for (size_t k = useLen; k < fftLen; ++k) buf[k] = Complex(0.0, 0.0);
            if (dir == -1) {
                for (auto &v : buf) v = std::conj(v);
                detail::fftRadix2Impl(buf.data(), fftLen, W.data());
                const double invN = 1.0 / static_cast<double>(fftLen);
                for (auto &v : buf) v = std::conj(v) * invN;
            } else {
                detail::fftRadix2Impl(buf.data(), fftLen, W.data());
            }
            // Store contiguous (outAxisStride == 1 when dim==1)
            for (size_t k = 0; k < outAxisLen; ++k) dst[outBase + k] = buf[k];
        }
    } else {
        // dim == 2 or dim == 3 — non-unit strides, fused generic loop.
        const size_t o1Len    = (dim == 2) ? R : R;      // inner "other" dim
        const size_t o1Stride = 1;
        const size_t o2Len    = (dim == 2) ? P : C;      // outer "other" dim
        const size_t o2Stride = (dim == 2) ? (R * C) : R;
        const size_t o2OutStride = (dim == 2) ? (outR * outC) : outR;

        for (size_t i2 = 0; i2 < o2Len; ++i2) {
            for (size_t i1 = 0; i1 < o1Len; ++i1) {
                const size_t inBase  = i1 * o1Stride + i2 * o2Stride;
                const size_t outBase = i1 * o1Stride + i2 * o2OutStride;
                if (srcIsComplex) {
                    for (size_t k = 0; k < useLen; ++k)
                        buf[k] = srcC[inBase + k * axisStride];
                } else {
                    for (size_t k = 0; k < useLen; ++k)
                        buf[k] = Complex(srcD[inBase + k * axisStride], 0.0);
                }
                for (size_t k = useLen; k < fftLen; ++k) buf[k] = Complex(0.0, 0.0);
                if (dir == -1) {
                    for (auto &v : buf) v = std::conj(v);
                    fftRadix2(buf, 1);
                    const double invN = 1.0 / static_cast<double>(fftLen);
                    for (auto &v : buf) v = std::conj(v) * invN;
                } else {
                    fftRadix2(buf, 1);
                }
                for (size_t k = 0; k < outAxisLen; ++k)
                    dst[outBase + k * outAxisStride] = buf[k];
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
