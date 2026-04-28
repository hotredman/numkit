// libs/builtin/src/backends/compare_portable.cpp
//
// Scalar reference for the comparison fast path. Compiled when
// NUMKIT_WITH_SIMD=OFF. Same dispatch contract as the Highway variant —
// returns LOGICAL output for pure DOUBLE × DOUBLE inputs (or DOUBLE
// scalar broadcast), otherwise returns an unset Value so the generic
// `compareImpl` in MStdBinaryOps.cpp handles it.

#include "compare.hpp"

#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include "../helpers.hpp"

#include <cstddef>
#include <cstdint>

namespace numkit::builtin {
namespace {

bool isDoubleish(const Value &v)
{
    return v.type() == ValueType::DOUBLE && !v.isComplex();
}

bool compatibleShape(const Value &a, const Value &b)
{
    if (a.isScalar() || b.isScalar()) return true;
    return a.dims() == b.dims();
}

template <typename Op>
Value dispatchFast(const Value &a, const Value &b, Op op)
{
    if (!isDoubleish(a) || !isDoubleish(b)) return Value{};
    if (!compatibleShape(a, b))             return Value{};
    if (a.isScalar() && b.isScalar())       return Value{};

    if (a.isScalar()) {
        const double s = a.toScalar();
        const double *pb = b.doubleData();
        auto r = createLike(b, ValueType::LOGICAL, nullptr);
        uint8_t *out = r.logicalDataMut();
        const std::size_t n = b.numel();
        for (std::size_t i = 0; i < n; ++i)
            out[i] = op(s, pb[i]) ? 1 : 0;
        return r;
    }
    if (b.isScalar()) {
        const double s = b.toScalar();
        const double *pa = a.doubleData();
        auto r = createLike(a, ValueType::LOGICAL, nullptr);
        uint8_t *out = r.logicalDataMut();
        const std::size_t n = a.numel();
        for (std::size_t i = 0; i < n; ++i)
            out[i] = op(pa[i], s) ? 1 : 0;
        return r;
    }
    const double *pa = a.doubleData();
    const double *pb = b.doubleData();
    auto r = createLike(a, ValueType::LOGICAL, nullptr);
    uint8_t *out = r.logicalDataMut();
    const std::size_t n = a.numel();
    for (std::size_t i = 0; i < n; ++i)
        out[i] = op(pa[i], pb[i]) ? 1 : 0;
    return r;
}

} // namespace

Value eqFast(const Value &a, const Value &b) { return dispatchFast(a, b, [](double x, double y){ return x == y; }); }
Value neFast(const Value &a, const Value &b) { return dispatchFast(a, b, [](double x, double y){ return x != y; }); }
Value ltFast(const Value &a, const Value &b) { return dispatchFast(a, b, [](double x, double y){ return x <  y; }); }
Value gtFast(const Value &a, const Value &b) { return dispatchFast(a, b, [](double x, double y){ return x >  y; }); }
Value leFast(const Value &a, const Value &b) { return dispatchFast(a, b, [](double x, double y){ return x <= y; }); }
Value geFast(const Value &a, const Value &b) { return dispatchFast(a, b, [](double x, double y){ return x >= y; }); }

} // namespace numkit::builtin
