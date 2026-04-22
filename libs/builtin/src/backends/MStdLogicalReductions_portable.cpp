// libs/builtin/src/backends/MStdLogicalReductions_portable.cpp
//
// Scalar reference any() / all(). Compiled when NUMKIT_WITH_SIMD=OFF.
// The Highway-dispatched variant lives in MStdLogicalReductions_simd.cpp;
// behaviour is identical — same dim-dispatch, same LOGICAL output type,
// same MATLAB-empty semantics (any([])=false, all([])=true). Only the
// inner unit-stride scan loops differ (no Highway types here so the
// portable build doesn't need the Highway include path).

#include <numkit/m/builtin/MStdMatrix.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include "../MStdHelpers.hpp"
#include "../MStdReductionHelpers.hpp"

#include <complex>
#include <cstddef>
#include <cstdint>

namespace numkit::m::builtin {
namespace {

template <bool IsAny>
uint8_t scanGeneric(const MValue &x, std::size_t base, std::size_t n, std::size_t stride)
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
uint8_t scanSlice(const MValue &x, std::size_t base, std::size_t n, std::size_t stride)
{
    if (n == 0) return IsAny ? 0 : 1;
    if (x.type() == MType::DOUBLE) {
        const double *p = x.doubleData() + base;
        for (std::size_t i = 0; i < n; ++i) {
            const bool nz = p[i * stride] != 0.0;
            if constexpr (IsAny) { if (nz)  return 1; }
            else                 { if (!nz) return 0; }
        }
        return IsAny ? 0 : 1;
    }
    if (x.type() == MType::LOGICAL) {
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
MValue logicalReduceImpl(Allocator &alloc, const MValue &x, int dim)
{
    if (x.isEmpty())
        return MValue::logicalScalar(IsAny ? false : true, &alloc);
    if (x.isScalar()) {
        const bool nz = x.isComplex()
            ? (x.toComplex().real() != 0.0 || x.toComplex().imag() != 0.0)
            : (x.elemAsDouble(0) != 0.0);
        return MValue::logicalScalar(nz, &alloc);
    }
    if (x.dims().isVector()) {
        const uint8_t v = scanSlice<IsAny>(x, 0, x.numel(), 1);
        return MValue::logicalScalar(v != 0, &alloc);
    }

    const int d = detail::resolveDim(x, dim, IsAny ? "any" : "all");
    const auto outShape = detail::outShapeForDim(x, d);
    MValue out = createMatrix(outShape, MType::LOGICAL, &alloc);
    uint8_t *dst = out.logicalDataMut();

    const auto &dd = x.dims();
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

MValue anyOf(Allocator &alloc, const MValue &x, int dim)
{
    return logicalReduceImpl<true>(alloc, x, dim);
}

MValue allOf(Allocator &alloc, const MValue &x, int dim)
{
    return logicalReduceImpl<false>(alloc, x, dim);
}

} // namespace numkit::m::builtin
