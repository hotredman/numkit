// ops/include/numkit/ops/value_factory.hpp
//
// Small Value-shape factories shared by the op kernels. Built only on the
// Value substrate (value.hpp) — no engine, no toolbox. The element-wise
// kernels use these to allocate a result Value matching an input's shape.

#pragma once

#include <numkit/value/value.hpp>

#include <cstddef>
#include <memory_resource>

namespace numkit::ops {

// Create a (zero-filled) Value of `src`'s shape with the given element type.
inline Value createLike(const Value &src, ValueType type, std::pmr::memory_resource *mr)
{
    const Dims &d = src.dims();
    const int nd = d.ndim();
    if (nd >= 4) {
        std::size_t dims[Dims::kMaxRank];
        for (int i = 0; i < nd; ++i)
            dims[i] = d.dim(i);
        return Value::matrixND(dims, nd, type, mr);
    }
    if (d.is3D())
        return Value::matrix3d(d.rows(), d.cols(), d.pages(), type, mr);
    return Value::matrix(d.rows(), d.cols(), type, mr);
}

} // namespace numkit::ops
