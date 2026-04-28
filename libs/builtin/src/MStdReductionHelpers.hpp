// libs/builtin/src/MStdReductionHelpers.hpp
//
// Shared infrastructure for "reduce-along-dim" style operations.
// Used by Phase-1 stats (var/std/median/quantile/prctile/mode) and
// any future reductions that want explicit `dim` support.
//
// Two layouts handled:
//   * 2D matrix (R×C, column-major): dim=1 reduces rows, dim=2 reduces cols
//   * 3D array  (R×C×P, page = R*C stride): dim=1/2/3 reduces that axis
//
// Vectors and scalars: ignore the explicit dim and reduce all elements
// (matching MATLAB's "first non-singleton" behavior for vectors).
//
// API: applyAlongDim(x, dim, F) collects each slice into a contiguous
// scratch buffer and calls F(scratch.data(), n, out_index) → double.
// `out_index` is the linear index into the output buffer, useful when
// F needs to write multiple outputs per slice (e.g. mode wants both
// value and count — but we expose that via a separate API).

#pragma once

#include "MStdHelpers.hpp"

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace numkit::m::builtin::detail {

// Return MATLAB's "first non-singleton dim" (1-based). For an all-
// singleton input (scalar) returns 1. Used when the user omits `dim`.
inline int firstNonSingletonDim(const MValue &x)
{
    const auto &d = x.dims();
    const int nd = d.ndim();
    for (int i = 0; i < nd; ++i)
        if (d.dim(i) > 1) return i + 1;
    return 1;
}

// Validate user-supplied dim. dim must be a positive integer; dim
// past the input's actual ndim is treated as a trailing-singleton
// (returns sentinel = ndim+1 so callers know to do an identity copy).
inline int validateDim(const MValue &x, int dim, const char *fn)
{
    if (dim < 1)
        throw MError(std::string(fn) + ": dim must be a positive integer",
                     0, 0, fn, "", std::string("m:") + fn + ":badDim");
    const int nd = x.dims().ndim();
    if (dim > nd) {
        // Trailing-singleton semantics — caller produces an identity copy.
        return nd + 1;
    }
    return dim;
}

// Compute output shape for a reduce-along-dim operation. The reduced
// dim becomes 1; other dims preserved.
inline DimsArg outShapeForDim(const MValue &x, int dim)
{
    const auto &d = x.dims();
    DimsArg o{d.rows(), d.cols(), d.is3D() ? d.pages() : 0};
    switch (dim) {
        case 1: o.rows = 1; break;
        case 2: o.cols = 1; break;
        case 3: o.pages = (o.pages == 0) ? 0 : 1; break;
        default: break; // sentinel: no reduction (handled by caller)
    }
    return o;
}

// Shape of one slice (count of elements traversed when collapsing dim).
inline size_t sliceLenForDim(const MValue &x, int dim)
{
    const auto &d = x.dims();
    switch (dim) {
        case 1: return d.rows();
        case 2: return d.cols();
        case 3: return d.is3D() ? d.pages() : 1;
        default: return 1;
    }
}

// Number of independent slices we'll produce.
inline size_t sliceCountForDim(const MValue &x, int dim)
{
    return x.numel() / sliceLenForDim(x, dim);
}

// Iterate every (output_index, slice_data) pair. F receives a pointer
// to a contiguous buffer of length sliceLenForDim and writes outputs
// via `outBuf[outIdx] = …`. Non-DOUBLE inputs are read via
// elemAsDouble (DOUBLE keeps the contiguous-memcpy fast path).
//
// NOTE: F is called once per output element. For multi-output ops
// (like mode returning value+count) the caller must allocate two
// MValues and capture both buffers in F's closure.
template <typename F>
void forEachSlice(const MValue &x, int dim, F &&f)
{
    const auto &d = x.dims();
    const size_t R = d.rows(), C = d.cols(), P = d.is3D() ? d.pages() : 1;
    const size_t N = sliceLenForDim(x, dim);
    const bool fastDouble = (x.type() == MType::DOUBLE);
    const double *src = fastDouble ? x.doubleData() : nullptr;

    std::vector<double> scratch(N);

    if (dim == 1) {
        // Slice = a column (or column within a page). Stride = 1.
        size_t outIdx = 0;
        for (size_t pp = 0; pp < P; ++pp) {
            for (size_t c = 0; c < C; ++c) {
                const size_t base = pp * R * C + c * R;
                if (fastDouble) {
                    std::copy(src + base, src + base + N, scratch.data());
                } else {
                    for (size_t k = 0; k < N; ++k)
                        scratch[k] = x.elemAsDouble(base + k);
                }
                f(outIdx, scratch.data(), N);
                ++outIdx;
            }
        }
    } else if (dim == 2) {
        // Slice = a row (within a page). Stride = R.
        size_t outIdx = 0;
        for (size_t pp = 0; pp < P; ++pp) {
            for (size_t r = 0; r < R; ++r) {
                if (fastDouble) {
                    for (size_t c = 0; c < C; ++c)
                        scratch[c] = src[pp * R * C + c * R + r];
                } else {
                    for (size_t c = 0; c < C; ++c)
                        scratch[c] = x.elemAsDouble(pp * R * C + c * R + r);
                }
                f(outIdx, scratch.data(), N);
                ++outIdx;
            }
        }
    } else if (dim == 3) {
        // Slice = pages at fixed (r,c). Stride = R*C. Only meaningful for 3D.
        size_t outIdx = 0;
        for (size_t c = 0; c < C; ++c) {
            for (size_t r = 0; r < R; ++r) {
                if (fastDouble) {
                    for (size_t pp = 0; pp < P; ++pp)
                        scratch[pp] = src[pp * R * C + c * R + r];
                } else {
                    for (size_t pp = 0; pp < P; ++pp)
                        scratch[pp] = x.elemAsDouble(pp * R * C + c * R + r);
                }
                f(outIdx, scratch.data(), N);
                ++outIdx;
            }
        }
    }
}

// ND output-shape helper (any rank): copies all input dims, sets the
// reduced dim to 1. dim is 1-based; dim > ndim means trailing
// singleton (identity reduction) — caller handles separately.
inline std::vector<size_t> outShapeForDimND(const MValue &x, int dim)
{
    const auto &d = x.dims();
    std::vector<size_t> shape(d.ndim());
    for (int i = 0; i < d.ndim(); ++i)
        shape[i] = (i + 1 == dim) ? 1 : d.dim(i);
    return shape;
}

// ND apply core: walks each axis-`redAxis` slice via stride arithmetic
// and feeds it to F. Output is dense column-major matching the
// reduced-dim shape. Output is always DOUBLE (matches MATLAB convention
// for sum/mean/etc.); non-DOUBLE input is read via elemAsDouble so
// integer / single / logical inputs reduce correctly.
template <typename F>
void forEachSliceND(const MValue &x, int dim, double *dst, F &&f)
{
    const auto &d = x.dims();
    const int redAxis = dim - 1;
    const size_t sliceLen = d.dim(redAxis);
    size_t B = 1;
    for (int i = 0; i < redAxis; ++i) B *= d.dim(i);
    size_t O = 1;
    for (int i = redAxis + 1; i < d.ndim(); ++i) O *= d.dim(i);

    const bool fastDouble = (x.type() == MType::DOUBLE);
    const double *src = fastDouble ? x.doubleData() : nullptr;
    std::vector<double> scratch(sliceLen);
    if (B == 1) {
        // Reducing axis 0 → contiguous gather.
        if (fastDouble) {
            for (size_t o = 0; o < O; ++o) {
                const double *base = src + o * sliceLen;
                std::copy(base, base + sliceLen, scratch.data());
                dst[o] = f(o, scratch.data(), sliceLen);
            }
        } else {
            for (size_t o = 0; o < O; ++o) {
                const size_t base = o * sliceLen;
                for (size_t k = 0; k < sliceLen; ++k)
                    scratch[k] = x.elemAsDouble(base + k);
                dst[o] = f(o, scratch.data(), sliceLen);
            }
        }
        return;
    }
    for (size_t o = 0; o < O; ++o) {
        for (size_t b = 0; b < B; ++b) {
            const size_t base = o * sliceLen * B + b;
            if (fastDouble) {
                for (size_t k = 0; k < sliceLen; ++k)
                    scratch[k] = src[base + k * B];
            } else {
                for (size_t k = 0; k < sliceLen; ++k)
                    scratch[k] = x.elemAsDouble(base + k * B);
            }
            const size_t outIdx = o * B + b;
            dst[outIdx] = f(outIdx, scratch.data(), sliceLen);
        }
    }
}

// applyAlongDim: most common case — F maps a slice to a single double.
// Returns an MValue with the dim collapsed to 1. dim==-1 means "reduce
// all elements to a single scalar" (vector/scalar case).
template <typename F>
MValue applyAlongDim(const MValue &x, int dim, F &&f, Allocator *alloc)
{
    // 2D/3D empty: collapse to 0×0 (legacy behaviour; MATLAB's
    // shape-preservation for empty 2D inputs was never wired up here).
    // ND empty (rank ≥ 4): fall through to the ND path, which produces
    // a correctly-shaped output (e.g. sum(zeros([2,0,3,2]), 2) → 2×1×3×2).
    if (x.isEmpty() && x.dims().ndim() < 4) {
        return MValue::matrix(0, 0, MType::DOUBLE, alloc);
    }
    if (x.dims().isVector() || x.isScalar()) {
        std::vector<double> scratch(x.numel());
        if (x.type() == MType::DOUBLE)
            std::copy(x.doubleData(), x.doubleData() + x.numel(), scratch.data());
        else
            for (size_t i = 0; i < x.numel(); ++i)
                scratch[i] = x.elemAsDouble(i);
        return MValue::scalar(f(0, scratch.data(), x.numel()), alloc);
    }
    // ND fallback for rank ≥ 4. dim out-of-range was already mapped by
    // validateDim → ndim+1 sentinel, but we may receive dim ∈ [1, ndim].
    if (x.dims().ndim() >= 4 && dim >= 1 && dim <= x.dims().ndim()) {
        auto shape = outShapeForDimND(x, dim);
        MValue out = MValue::matrixND(shape.data(),
                                      static_cast<int>(shape.size()),
                                      MType::DOUBLE, alloc);
        double *dst = out.doubleDataMut();
        forEachSliceND(x, dim, dst, std::forward<F>(f));
        return out;
    }
    auto outShape = outShapeForDim(x, dim);
    MValue out = createMatrix(outShape, MType::DOUBLE, alloc);
    double *dst = out.doubleDataMut();
    forEachSlice(x, dim, [&](size_t outIdx, double *slice, size_t n) {
        dst[outIdx] = f(outIdx, slice, n);
    });
    return out;
}

// Index-aware variant: F writes (value, 1-based index of source element)
// per slice. Used by min/max with dim. Returns (values, indices).
template <typename F>
std::pair<MValue, MValue>
applyAlongDimWithIndex(const MValue &x, int dim, F &&f, Allocator *alloc)
{
    if (x.isEmpty() && x.dims().ndim() < 4) {
        return {MValue::matrix(0, 0, MType::DOUBLE, alloc),
                MValue::matrix(0, 0, MType::DOUBLE, alloc)};
    }
    if (x.dims().isVector() || x.isScalar()) {
        std::vector<double> scratch(x.numel());
        if (x.type() == MType::DOUBLE)
            std::copy(x.doubleData(), x.doubleData() + x.numel(), scratch.data());
        else
            for (size_t i = 0; i < x.numel(); ++i)
                scratch[i] = x.elemAsDouble(i);
        double v = 0;
        size_t idx = 0;
        f(scratch.data(), x.numel(), v, idx);
        return {MValue::scalar(v, alloc),
                MValue::scalar(static_cast<double>(idx + 1), alloc)};
    }
    // ND fallback (rank ≥ 4)
    if (x.dims().ndim() >= 4 && dim >= 1 && dim <= x.dims().ndim()) {
        auto shape = outShapeForDimND(x, dim);
        MValue out    = MValue::matrixND(shape.data(),
                                         static_cast<int>(shape.size()),
                                         MType::DOUBLE, alloc);
        MValue outIdx = MValue::matrixND(shape.data(),
                                         static_cast<int>(shape.size()),
                                         MType::DOUBLE, alloc);
        double *dst  = out.doubleDataMut();
        double *dstI = outIdx.doubleDataMut();
        forEachSliceND(x, dim, dst,
            [&](size_t outOff, double *slice, size_t n) {
                double v = 0; size_t idx = 0;
                f(slice, n, v, idx);
                dstI[outOff] = static_cast<double>(idx + 1);
                return v;
            });
        return {std::move(out), std::move(outIdx)};
    }
    auto outShape = outShapeForDim(x, dim);
    MValue out = createMatrix(outShape, MType::DOUBLE, alloc);
    MValue outIdx = createMatrix(outShape, MType::DOUBLE, alloc);
    double *dst = out.doubleDataMut();
    double *dstI = outIdx.doubleDataMut();
    forEachSlice(x, dim, [&](size_t outOff, double *slice, size_t n) {
        double v = 0;
        size_t idx = 0;
        f(slice, n, v, idx);
        dst[outOff] = v;
        dstI[outOff] = static_cast<double>(idx + 1);
    });
    return {std::move(out), std::move(outIdx)};
}

// Resolve effective dim: explicit user-supplied if provided, else the
// "first non-singleton" rule. Returns 1-based dim or 0 for "scalar".
inline int resolveDim(const MValue &x, int explicitDim, const char *fn)
{
    if (explicitDim > 0) return validateDim(x, explicitDim, fn);
    return firstNonSingletonDim(x);
}

// Compact NaN-free elements to the front of [data, data+n). Returns the
// new count k (0 <= k <= n). Used by nan* reductions: after compacting
// they call the regular reduction kernel on [data, data+k) and decide
// what to return when k == 0 (which depends on the function — nansum
// returns 0, nanmean/nanvar/nanstd/nanmedian/nanmax/nanmin return NaN).
inline size_t compactNonNan(double *data, size_t n)
{
    size_t w = 0;
    for (size_t i = 0; i < n; ++i) {
        if (!std::isnan(data[i])) {
            if (w != i) data[w] = data[i];
            ++w;
        }
    }
    return w;
}

} // namespace numkit::m::builtin::detail
