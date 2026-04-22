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
    if (d.rows() > 1) return 1;
    if (d.cols() > 1) return 2;
    if (d.is3D() && d.pages() > 1) return 3;
    return 1;
}

// Validate user-supplied dim. MATLAB allows dim > ndims (treated as a
// trailing singleton). For Phase 1 we accept 1..3 only — extending to
// >ndims-as-singleton requires returning the input shape unchanged,
// which we'll add when the first script needs it.
inline int validateDim(const MValue &x, int dim, const char *fn)
{
    if (dim < 1)
        throw MError(std::string(fn) + ": dim must be a positive integer",
                     0, 0, fn, "", std::string("m:") + fn + ":badDim");
    const int maxDim = x.dims().is3D() ? 3 : 2;
    if (dim > maxDim) {
        // Trailing-singleton semantics: reducing along a dim of size 1
        // is a no-op for value-producing reductions but still requires
        // the output shape to match. Phase 1: clamp to maxDim and let
        // the slice-of-length-1 case produce identity.
        return maxDim + 1; // sentinel — caller treats as "no reduction"
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
// via `outBuf[outIdx] = …`.
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
    const double *src = x.doubleData();

    std::vector<double> scratch(N);

    if (dim == 1) {
        // Slice = a column (or column within a page). Stride = 1.
        size_t outIdx = 0;
        for (size_t pp = 0; pp < P; ++pp) {
            for (size_t c = 0; c < C; ++c) {
                const double *base = src + pp * R * C + c * R;
                std::copy(base, base + N, scratch.data());
                f(outIdx, scratch.data(), N);
                ++outIdx;
            }
        }
    } else if (dim == 2) {
        // Slice = a row (within a page). Stride = R.
        // Output shape is (R, 1, P) column-major: dst[pp*R + r] —
        // walk r in inner loop so outIdx increments contiguously.
        size_t outIdx = 0;
        for (size_t pp = 0; pp < P; ++pp) {
            for (size_t r = 0; r < R; ++r) {
                for (size_t c = 0; c < C; ++c)
                    scratch[c] = src[pp * R * C + c * R + r];
                f(outIdx, scratch.data(), N);
                ++outIdx;
            }
        }
    } else if (dim == 3) {
        // Slice = pages at fixed (r,c). Stride = R*C. Only meaningful for 3D.
        size_t outIdx = 0;
        for (size_t c = 0; c < C; ++c) {
            for (size_t r = 0; r < R; ++r) {
                for (size_t pp = 0; pp < P; ++pp)
                    scratch[pp] = src[pp * R * C + c * R + r];
                f(outIdx, scratch.data(), N);
                ++outIdx;
            }
        }
    }
}

// applyAlongDim: most common case — F maps a slice to a single double.
// Returns an MValue with the dim collapsed to 1. dim==-1 means "reduce
// all elements to a single scalar" (vector/scalar case).
template <typename F>
MValue applyAlongDim(const MValue &x, int dim, F &&f, Allocator *alloc)
{
    if (x.isEmpty()) {
        // MATLAB: sum/mean/var of empty = various things. Caller decides
        // by passing an empty-handler in F when needed. Default = 0×0.
        return MValue::matrix(0, 0, MType::DOUBLE, alloc);
    }
    if (x.dims().isVector() || x.isScalar()) {
        std::vector<double> scratch(x.numel());
        std::copy(x.doubleData(), x.doubleData() + x.numel(), scratch.data());
        return MValue::scalar(f(0, scratch.data(), x.numel()), alloc);
    }
    auto outShape = outShapeForDim(x, dim);
    MValue out = createMatrix(outShape, MType::DOUBLE, alloc);
    double *dst = out.doubleDataMut();
    forEachSlice(x, dim, [&](size_t outIdx, double *slice, size_t n) {
        dst[outIdx] = f(outIdx, slice, n);
    });
    return out;
}

// Two-output variant: F writes (value, secondary) per slice. Used by
// mode (value + frequency). Returns a tuple of (out, out2).
template <typename F>
std::pair<MValue, MValue>
applyAlongDimPair(const MValue &x, int dim, F &&f, Allocator *alloc)
{
    if (x.isEmpty()) {
        return {MValue::matrix(0, 0, MType::DOUBLE, alloc),
                MValue::matrix(0, 0, MType::DOUBLE, alloc)};
    }
    if (x.dims().isVector() || x.isScalar()) {
        std::vector<double> scratch(x.numel());
        std::copy(x.doubleData(), x.doubleData() + x.numel(), scratch.data());
        double v = 0, s = 0;
        f(scratch.data(), x.numel(), v, s);
        return {MValue::scalar(v, alloc), MValue::scalar(s, alloc)};
    }
    auto outShape = outShapeForDim(x, dim);
    MValue out = createMatrix(outShape, MType::DOUBLE, alloc);
    MValue out2 = createMatrix(outShape, MType::DOUBLE, alloc);
    double *d1 = out.doubleDataMut();
    double *d2 = out2.doubleDataMut();
    forEachSlice(x, dim, [&](size_t outIdx, double *slice, size_t n) {
        double v = 0, s = 0;
        f(slice, n, v, s);
        d1[outIdx] = v;
        d2[outIdx] = s;
    });
    return {std::move(out), std::move(out2)};
}

// Resolve effective dim: explicit user-supplied if provided, else the
// "first non-singleton" rule. Returns 1-based dim or 0 for "scalar".
inline int resolveDim(const MValue &x, int explicitDim, const char *fn)
{
    if (explicitDim > 0) return validateDim(x, explicitDim, fn);
    return firstNonSingletonDim(x);
}

} // namespace numkit::m::builtin::detail
