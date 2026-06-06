// ops/include/numkit/ops/value_factory.hpp
//
// Value-shape factories shared by the op kernels and the toolboxes: allocate a
// Value of a given shape/type. Built only on the Value substrate (value.hpp) —
// no engine, no toolbox — so they live at the L0.5 ops layer and every layer
// above can use them.
//
// (Historically in libs/builtin/src/helpers.hpp under namespace numkit;
// helpers.hpp now re-exports these into numkit:: so existing unqualified
// callers are unaffected.)

#pragma once

#include <numkit/value/value.hpp>

#include <cstddef>
#include <memory_resource>

namespace numkit::ops {

// 2D/3D shape descriptor for createMatrix (pages == 0 → 2D).
struct DimsArg
{
    std::size_t rows = 1, cols = 1, pages = 0;
};

// Create a zero matrix / 3D array with the given dimensions + element type.
inline Value createMatrix(DimsArg d, ValueType type, std::pmr::memory_resource *mr)
{
    if (d.pages > 0)
        return Value::matrix3d(d.rows, d.cols, d.pages, type, mr);
    return Value::matrix(d.rows, d.cols, type, mr);
}

// Allocate a Value with the given Dims, picking the rank-appropriate ctor
// (matrix / matrix3d / matrixND).
inline Value createForDims(const Dims &d, ValueType type, std::pmr::memory_resource *mr)
{
    const int nd = d.ndim();
    if (nd >= 4) {
        std::size_t dims[Dims::kMaxRank];
        for (int i = 0; i < nd; ++i)
            dims[i] = d.dim(i);
        return Value::matrixND(dims, nd, type, mr);
    }
    return createMatrix({d.rows(), d.cols(), d.is3D() ? d.pages() : 0}, type, mr);
}

// Allocate a Value with the same shape as `src`, optionally of a different
// type. Required for any elementwise output (a numel()-into-2D allocation
// corrupts the heap when src is 3D+).
inline Value createLike(const Value &src, ValueType type, std::pmr::memory_resource *mr)
{
    return createForDims(src.dims(), type, mr);
}

} // namespace numkit::ops
