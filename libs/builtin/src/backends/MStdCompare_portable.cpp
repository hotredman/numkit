// libs/builtin/src/backends/MStdCompare_portable.cpp
//
// Scalar reference for the comparison fast path. Compiled when
// NUMKIT_WITH_SIMD=OFF. Same dispatch contract as the Highway variant —
// returns LOGICAL output for pure DOUBLE × DOUBLE inputs (or DOUBLE
// scalar broadcast), otherwise returns an unset MValue so the generic
// `compareImpl` in MStdBinaryOps.cpp handles it.

#include "MStdCompare.hpp"

#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include "../MStdHelpers.hpp"

#include <cstddef>
#include <cstdint>

namespace numkit::m::builtin {
namespace {

bool isDoubleish(const MValue &v)
{
    return v.type() == MType::DOUBLE && !v.isComplex();
}

bool compatibleShape(const MValue &a, const MValue &b)
{
    if (a.isScalar() || b.isScalar()) return true;
    return a.dims() == b.dims();
}

template <typename Op>
MValue dispatchFast(const MValue &a, const MValue &b, Op op)
{
    if (!isDoubleish(a) || !isDoubleish(b)) return MValue{};
    if (!compatibleShape(a, b))             return MValue{};
    if (a.isScalar() && b.isScalar())       return MValue{};

    if (a.isScalar()) {
        const double s = a.toScalar();
        const double *pb = b.doubleData();
        auto r = createLike(b, MType::LOGICAL, nullptr);
        uint8_t *out = r.logicalDataMut();
        const std::size_t n = b.numel();
        for (std::size_t i = 0; i < n; ++i)
            out[i] = op(s, pb[i]) ? 1 : 0;
        return r;
    }
    if (b.isScalar()) {
        const double s = b.toScalar();
        const double *pa = a.doubleData();
        auto r = createLike(a, MType::LOGICAL, nullptr);
        uint8_t *out = r.logicalDataMut();
        const std::size_t n = a.numel();
        for (std::size_t i = 0; i < n; ++i)
            out[i] = op(pa[i], s) ? 1 : 0;
        return r;
    }
    const double *pa = a.doubleData();
    const double *pb = b.doubleData();
    auto r = createLike(a, MType::LOGICAL, nullptr);
    uint8_t *out = r.logicalDataMut();
    const std::size_t n = a.numel();
    for (std::size_t i = 0; i < n; ++i)
        out[i] = op(pa[i], pb[i]) ? 1 : 0;
    return r;
}

} // namespace

MValue eqFast(const MValue &a, const MValue &b) { return dispatchFast(a, b, [](double x, double y){ return x == y; }); }
MValue neFast(const MValue &a, const MValue &b) { return dispatchFast(a, b, [](double x, double y){ return x != y; }); }
MValue ltFast(const MValue &a, const MValue &b) { return dispatchFast(a, b, [](double x, double y){ return x <  y; }); }
MValue gtFast(const MValue &a, const MValue &b) { return dispatchFast(a, b, [](double x, double y){ return x >  y; }); }
MValue leFast(const MValue &a, const MValue &b) { return dispatchFast(a, b, [](double x, double y){ return x <= y; }); }
MValue geFast(const MValue &a, const MValue &b) { return dispatchFast(a, b, [](double x, double y){ return x >= y; }); }

} // namespace numkit::m::builtin
