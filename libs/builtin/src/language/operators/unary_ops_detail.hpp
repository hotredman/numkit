// libs/.../unary_ops_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by unary_ops.cpp + unary_ops_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include "reduction_helpers.hpp"  // engine-free numkit::builtin::detail dim-infra (ops re-export)

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

// Generic 2-D transpose shared by `.'` (transposeNC) and `'`
// (ctranspose). `conjugate` only affects COMPLEX input (negate the
// imaginary part). Type-preserving across DOUBLE / SINGLE / CHAR /
// LOGICAL / integer (raw byte rearrange), COMPLEX (per-element, optional
// conjugate) and CELL (per-cell move). STRING / STRUCT / FUNC_HANDLE are
// unsupported. Matches MATLAB: `A.'` / `A'` / transpose(A) / ctranspose(A)
// preserve the input class for all of these.
Value transpose2D(const Value &x, bool conjugate, const char *fnName,
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
        // r(j,i) = x(i,j): r idx (col-major, cols rows) = i*cols + j;
        //                  x idx (col-major, rows rows) = j*rows + i.
        for (size_t i = 0; i < rows; ++i)
            for (size_t j = 0; j < cols; ++j)
                r.cellAt(i * cols + j) = x.cellAt(j * rows + i);
        return r;
    }

    // OBJECT arrays transpose like CELL (conjugate is a no-op for objects).
    if (t == ValueType::OBJECT)
        return x.objectTranspose(p);

    if (t == ValueType::STRING || t == ValueType::STRUCT ||
        t == ValueType::FUNC_HANDLE)
        throw Error("Transpose not supported for this type",
                     0, 0, fnName, "", "numkit:transpose:unsupportedType");

    // POD path: DOUBLE / SINGLE / CHAR / LOGICAL / int* — raw bytes.
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
