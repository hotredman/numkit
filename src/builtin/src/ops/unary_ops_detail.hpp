// src/builtin/src/ops/unary_ops_detail.hpp
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include <numkit/ops/reductions.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::builtin {

namespace {

inline int8_t saturateNeg(int8_t v) {
    if (v == std::numeric_limits<int8_t>::min()) return std::numeric_limits<int8_t>::max();
    return static_cast<int8_t>(-v);
}
inline int16_t saturateNeg(int16_t v) {
    if (v == std::numeric_limits<int16_t>::min()) return std::numeric_limits<int16_t>::max();
    return static_cast<int16_t>(-v);
}
inline int32_t saturateNeg(int32_t v) {
    if (v == std::numeric_limits<int32_t>::min()) return std::numeric_limits<int32_t>::max();
    return static_cast<int32_t>(-v);
}
inline int64_t saturateNeg(int64_t v) {
    if (v == std::numeric_limits<int64_t>::min()) return std::numeric_limits<int64_t>::max();
    return static_cast<int64_t>(-v);
}

inline Value transpose2D(const Value &x, bool conjugate, const char *fnName,
                         std::pmr::memory_resource *p)
{
    if (x.dims().is3D())
        throw Error("transpose is not defined for N-D arrays",
                     0, 0, fnName, "", "numkit:transpose:3DInput");
    const size_t rows = x.dims().rows(), cols = x.dims().cols();
    const ValueType t = x.type();

    if (t == ValueType::COMPLEX) {
        if (x.isScalar())
            return Value::complexScalar(conjugate ? std::conj(x.toComplex())
                                                  : x.toComplex(), p);
        auto r = Value::complexMatrix(cols, rows, p);
        Complex *dst = r.complexDataMut();
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j) {
                const Complex v = x.complexElem(i, j);
                dst[i * cols + j] = conjugate ? std::conj(v) : v;
            }
        return r;
    }

    if (t == ValueType::CELL) {
        auto r = Value::cell(cols, rows, p);
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j)
                r.cellAt(i * cols + j) = x.cellAt(j * rows + i);
        return r;
    }

    if (t == ValueType::OBJECT)
        return x.objectTranspose(p);

    if (t == ValueType::STRING || t == ValueType::STRUCT ||
        t == ValueType::FUNC_HANDLE)
        throw Error("Transpose not supported for this type",
                     0, 0, fnName, "", "numkit:transpose:unsupportedType");

    if (t == ValueType::DOUBLE && x.isScalar())
        return Value::scalar(x.toScalar(), p);
    const size_t es = elementSize(t);
    auto r = Value::matrix(cols, rows, t, p);
    const char *src = static_cast<const char *>(x.rawData());
    char *dst = static_cast<char *>(r.rawDataMut());
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            std::memcpy(dst + (i * cols + j) * es,
                        src + (j * rows + i) * es, es);
    return r;
}

} // namespace

} // namespace numkit::builtin
