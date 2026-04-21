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

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace numkit::m::dsp {

// ── Shared algorithm core ──────────────────────────────────────────────
//
// dir = +1 for forward FFT, -1 for inverse. Unifies fft() and ifft() — they
// only differ in pre/post-conjugation and the 1/N scale for the inverse.
static MValue fftAlongDim(const MValue &x, size_t N, int dim, int dir, Allocator *alloc)
{
    const auto &dims = x.dims();
    const size_t rows = dims.rows(), cols = dims.cols();

    // ── Vector case (rows == 1 or cols == 1) ───────────────────────────
    if (rows == 1 || cols == 1) {
        const size_t inputLen = x.numel();
        const size_t outLen = (N > 0) ? N : inputLen;
        const size_t useLen = std::min(inputLen, outLen);
        const size_t fftLen = nextPow2(outLen);
        auto buf = prepareFFTBuffer(x, useLen, fftLen);

        if (dir == -1) {
            for (auto &v : buf)
                v = std::conj(v);
            fftRadix2(buf, 1);
            const double invN = 1.0 / static_cast<double>(fftLen);
            for (auto &v : buf)
                v = std::conj(v) * invN;

            // If result is real within numerical tolerance, return as real
            bool isReal = true;
            for (size_t i = 0; i < outLen && isReal; ++i)
                if (std::abs(buf[i].imag()) > 1e-10)
                    isReal = false;
            if (isReal) {
                const bool isRow = (rows == 1);
                auto r = isRow ? MValue::matrix(1, outLen, MType::DOUBLE, alloc)
                               : MValue::matrix(outLen, 1, MType::DOUBLE, alloc);
                for (size_t i = 0; i < outLen; ++i)
                    r.doubleDataMut()[i] = buf[i].real();
                return r;
            }
            const bool isRow = (rows == 1);
            auto r = isRow ? MValue::complexMatrix(1, outLen, alloc)
                           : MValue::complexMatrix(outLen, 1, alloc);
            for (size_t i = 0; i < outLen; ++i)
                r.complexDataMut()[i] = buf[i];
            return r;
        }

        fftRadix2(buf, 1);
        const bool isRow = (rows == 1);
        auto r = isRow ? MValue::complexMatrix(1, outLen, alloc)
                       : MValue::complexMatrix(outLen, 1, alloc);
        for (size_t i = 0; i < outLen; ++i)
            r.complexDataMut()[i] = buf[i];
        return r;
    }

    // ── Matrix case: column-wise (dim==1) or row-wise (dim==2) ─────────
    if (dim == 1) {
        // FFT along columns
        const size_t outRows = (N > 0) ? N : rows;
        auto result = MValue::complexMatrix(outRows, cols, alloc);
        Complex *dst = result.complexDataMut();
        for (size_t c = 0; c < cols; ++c) {
            const size_t useLen = std::min(rows, outRows);
            const size_t fftLen = nextPow2(outRows);
            std::vector<Complex> buf(fftLen, Complex(0.0, 0.0));
            if (x.isComplex()) {
                const Complex *src = x.complexData();
                for (size_t r = 0; r < useLen; ++r)
                    buf[r] = src[c * rows + r];
            } else {
                const double *src = x.doubleData();
                for (size_t r = 0; r < useLen; ++r)
                    buf[r] = Complex(src[c * rows + r], 0.0);
            }
            if (dir == -1) {
                for (auto &v : buf)
                    v = std::conj(v);
                fftRadix2(buf, 1);
                const double invN = 1.0 / static_cast<double>(fftLen);
                for (auto &v : buf)
                    v = std::conj(v) * invN;
            } else {
                fftRadix2(buf, 1);
            }
            for (size_t r = 0; r < outRows; ++r)
                dst[c * outRows + r] = buf[r];
        }
        return result;
    } else {
        // FFT along rows (dim == 2)
        const size_t outCols = (N > 0) ? N : cols;
        auto result = MValue::complexMatrix(rows, outCols, alloc);
        Complex *dst = result.complexDataMut();
        for (size_t r = 0; r < rows; ++r) {
            const size_t useLen = std::min(cols, outCols);
            const size_t fftLen = nextPow2(outCols);
            std::vector<Complex> buf(fftLen, Complex(0.0, 0.0));
            if (x.isComplex()) {
                const Complex *src = x.complexData();
                for (size_t c = 0; c < useLen; ++c)
                    buf[c] = src[c * rows + r];
            } else {
                const double *src = x.doubleData();
                for (size_t c = 0; c < useLen; ++c)
                    buf[c] = Complex(src[c * rows + r], 0.0);
            }
            if (dir == -1) {
                for (auto &v : buf)
                    v = std::conj(v);
                fftRadix2(buf, 1);
                const double invN = 1.0 / static_cast<double>(fftLen);
                for (auto &v : buf)
                    v = std::conj(v) * invN;
            } else {
                fftRadix2(buf, 1);
            }
            for (size_t c = 0; c < outCols; ++c)
                dst[c * rows + r] = buf[c];
        }
        return result;
    }
}

// ── Public API ─────────────────────────────────────────────────────────

MValue fft(Allocator &alloc, const MValue &x, int n, int dim)
{
    if (dim != 1 && dim != 2)
        throw MError("fft: dim must be 1 or 2", 0, 0, "fft", "", "m:fft:invalidDim");

    const size_t N = (n < 0) ? 0u : static_cast<size_t>(n);
    return fftAlongDim(x, N, dim, /*dir=*/1, &alloc);
}

MValue ifft(Allocator &alloc, const MValue &X, int n, int dim)
{
    if (dim != 1 && dim != 2)
        throw MError("ifft: dim must be 1 or 2", 0, 0, "ifft", "", "m:ifft:invalidDim");

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
    int dim = 1;
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
    int dim = 1;
    if (args.size() >= 2 && !args[1].isEmpty())
        n = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3)
        dim = static_cast<int>(args[2].toScalar());

    outs[0] = ifft(ctx.engine->allocator(), args[0], n, dim);
}

} // namespace detail

} // namespace numkit::m::dsp
