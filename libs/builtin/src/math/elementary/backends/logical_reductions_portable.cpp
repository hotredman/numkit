// libs/builtin/src/math/elementary/backends/logical_reductions_portable.cpp
//
// Scalar reference any() / all(). Compiled when NUMKIT_WITH_SIMD=OFF.
// The Highway-dispatched variant lives in logical_reductions_simd.cpp;
// behaviour is identical — same dim-dispatch, same LOGICAL output type,
// same MATLAB-empty semantics (any([])=false, all([])=true). Only the
// inner unit-stride scan loops differ (no Highway types here so the
// portable build doesn't need the Highway include path).

#include <numkit/builtin/lang/arrays/matrix.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"

#include <complex>
#include <cstddef>
#include <cstdint>

namespace numkit::builtin {
namespace {

template <bool IsAny>
uint8_t scanGeneric(const Value &x, std::size_t base, std::size_t n, std::size_t stride)
{
    if (x.isComplex()) {
        const auto *p = x.complexData();
        for (std::size_t i = 0; i < n; ++i) {
            const auto z = p[base + i * stride];
            const bool nz = z.real() != 0.0 || z.imag() != 0.0;
            if constexpr (IsAny) { if (nz)  return 1; }
            else                 { if (!nz) return 0; }
        }
    } else {
        for (std::size_t i = 0; i < n; ++i) {
            const bool nz = x.elemAsDouble(base + i * stride) != 0.0;
            if constexpr (IsAny) { if (nz)  return 1; }
            else                 { if (!nz) return 0; }
        }
    }
    return IsAny ? 0 : 1;
}

template <bool IsAny>
uint8_t scanSlice(const Value &x, std::size_t base, std::size_t n, std::size_t stride)
{
    if (n == 0) return IsAny ? 0 : 1;
    if (x.type() == ValueType::DOUBLE) {
        const double *p = x.doubleData() + base;
        for (std::size_t i = 0; i < n; ++i) {
            const bool nz = p[i * stride] != 0.0;
            if constexpr (IsAny) { if (nz)  return 1; }
            else                 { if (!nz) return 0; }
        }
        return IsAny ? 0 : 1;
    }
    if (x.type() == ValueType::LOGICAL) {
        const uint8_t *p = x.logicalData() + base;
        for (std::size_t i = 0; i < n; ++i) {
            const bool nz = p[i * stride] != 0;
            if constexpr (IsAny) { if (nz)  return 1; }
            else                 { if (!nz) return 0; }
        }
        return IsAny ? 0 : 1;
    }
    return scanGeneric<IsAny>(x, base, n, stride);
}

template <bool IsAny>
Value logicalReduceImpl(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    if (x.isEmpty())
        return Value::logicalScalar(IsAny ? false : true, mr);
    if (x.isScalar()) {
        const bool nz = x.isComplex()
            ? (x.toComplex().real() != 0.0 || x.toComplex().imag() != 0.0)
            : (x.elemAsDouble(0) != 0.0);
        return Value::logicalScalar(nz, mr);
    }
    if (x.dims().isVector()) {
        const uint8_t v = scanSlice<IsAny>(x, 0, x.numel(), 1);
        return Value::logicalScalar(v != 0, mr);
    }

    const int d = detail::resolveDim(x, dim, IsAny ? "any" : "all");
    const auto &dd = x.dims();

    // ND fallback: rank ≥ 4 — stride arithmetic via scanSlice.
    if (dd.ndim() >= 4 && d >= 1 && d <= dd.ndim()) {
        ScratchArena scratch_arena(mr);
        auto shape = detail::outShapeForDimND(&scratch_arena, x, d);
        Value out = Value::matrixND(shape.data(),
                                      static_cast<int>(shape.size()),
                                      ValueType::LOGICAL, mr);
        uint8_t *dst = out.logicalDataMut();
        const std::size_t sliceLen = dd.dim(d - 1);
        std::size_t B = 1;
        for (int i = 0; i < d - 1; ++i) B *= dd.dim(i);
        std::size_t O = 1;
        for (int i = d; i < dd.ndim(); ++i) O *= dd.dim(i);
        for (std::size_t o = 0; o < O; ++o)
            for (std::size_t b = 0; b < B; ++b) {
                const std::size_t base = o * sliceLen * B + b;
                dst[o * B + b] = scanSlice<IsAny>(x, base, sliceLen, B);
            }
        return out;
    }

    const auto outShape = detail::outShapeForDim(x, d);
    Value out = createMatrix(outShape, ValueType::LOGICAL, mr);
    uint8_t *dst = out.logicalDataMut();

    const std::size_t R = dd.rows(), C = dd.cols(), P = dd.is3D() ? dd.pages() : 1;

    if (d == 1) {
        std::size_t outIdx = 0;
        for (std::size_t pp = 0; pp < P; ++pp)
            for (std::size_t c = 0; c < C; ++c) {
                const std::size_t base = pp * R * C + c * R;
                dst[outIdx++] = scanSlice<IsAny>(x, base, R, 1);
            }
    } else if (d == 2) {
        std::size_t outIdx = 0;
        for (std::size_t pp = 0; pp < P; ++pp)
            for (std::size_t r = 0; r < R; ++r) {
                const std::size_t base = pp * R * C + r;
                dst[outIdx++] = scanSlice<IsAny>(x, base, C, R);
            }
    } else if (d == 3) {
        std::size_t outIdx = 0;
        for (std::size_t c = 0; c < C; ++c)
            for (std::size_t r = 0; r < R; ++r) {
                const std::size_t base = c * R + r;
                dst[outIdx++] = scanSlice<IsAny>(x, base, P, R * C);
            }
    }
    return out;
}

} // namespace

Value anyOf(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    return logicalReduceImpl<true>(mr, x, dim);
}

Value allOf(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    return logicalReduceImpl<false>(mr, x, dim);
}

} // namespace numkit::builtin
