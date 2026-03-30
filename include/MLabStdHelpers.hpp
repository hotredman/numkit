#pragma once

#include "MLabEngine.hpp"

#include <cmath>
#include <complex>
#include <stdexcept>
#include <utility>

namespace mlab {

// ============================================================
// Helper: promote pair to complex if needed
// ============================================================
inline std::pair<MValue, MValue> promoteToComplex(const MValue &a, const MValue &b, Allocator *alloc)
{
    MValue ca = a, cb = b;
    if (a.isComplex() && !b.isComplex())
        cb.promoteToComplex(alloc);
    else if (!a.isComplex() && b.isComplex())
        ca.promoteToComplex(alloc);
    return {ca, cb};
}

// ============================================================
// Elementwise binary op on double arrays
// ============================================================
template<typename Op>
MValue elementwiseDouble(const MValue &a, const MValue &b, Op op, Allocator *alloc)
{
    if (a.isScalar() && b.isScalar())
        return MValue::scalar(op(a.toScalar(), b.toScalar()), alloc);
    if (a.isScalar()) {
        auto r = MValue::matrix(b.dims().rows(), b.dims().cols(), MType::DOUBLE, alloc);
        double av = a.toScalar();
        for (size_t i = 0; i < b.numel(); ++i)
            r.doubleDataMut()[i] = op(av, b.doubleData()[i]);
        return r;
    }
    if (b.isScalar()) {
        auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
        double bv = b.toScalar();
        for (size_t i = 0; i < a.numel(); ++i)
            r.doubleDataMut()[i] = op(a.doubleData()[i], bv);
        return r;
    }
    if (a.dims() != b.dims())
        throw std::runtime_error("Matrix dimensions must agree");
    auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
    for (size_t i = 0; i < a.numel(); ++i)
        r.doubleDataMut()[i] = op(a.doubleData()[i], b.doubleData()[i]);
    return r;
}

// ============================================================
// Elementwise binary op on complex arrays
// ============================================================
template<typename Op>
MValue elementwiseComplex(const MValue &a, const MValue &b, Op op, Allocator *alloc)
{
    auto [ca, cb] = promoteToComplex(a, b, alloc);
    if (ca.isScalar() && cb.isScalar())
        return MValue::complexScalar(op(ca.toComplex(), cb.toComplex()), alloc);
    if (ca.isScalar()) {
        auto r = MValue::complexMatrix(cb.dims().rows(), cb.dims().cols(), alloc);
        Complex av = ca.toComplex();
        for (size_t i = 0; i < cb.numel(); ++i)
            r.complexDataMut()[i] = op(av, cb.complexData()[i]);
        return r;
    }
    if (cb.isScalar()) {
        auto r = MValue::complexMatrix(ca.dims().rows(), ca.dims().cols(), alloc);
        Complex bv = cb.toComplex();
        for (size_t i = 0; i < ca.numel(); ++i)
            r.complexDataMut()[i] = op(ca.complexData()[i], bv);
        return r;
    }
    if (ca.dims() != cb.dims())
        throw std::runtime_error("Matrix dimensions must agree");
    auto r = MValue::complexMatrix(ca.dims().rows(), ca.dims().cols(), alloc);
    for (size_t i = 0; i < ca.numel(); ++i)
        r.complexDataMut()[i] = op(ca.complexData()[i], cb.complexData()[i]);
    return r;
}

// ============================================================
// Elementwise unary on double
// ============================================================
template<typename Op>
MValue unaryDouble(const MValue &a, Op op, Allocator *alloc)
{
    if (a.isScalar())
        return MValue::scalar(op(a.toScalar()), alloc);
    auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
    for (size_t i = 0; i < a.numel(); ++i)
        r.doubleDataMut()[i] = op(a.doubleData()[i]);
    return r;
}

// ============================================================
// Elementwise unary on complex
// ============================================================
template<typename Op>
MValue unaryComplex(const MValue &a, Op op, Allocator *alloc)
{
    if (a.isScalar())
        return MValue::complexScalar(op(a.toComplex()), alloc);
    auto r = MValue::complexMatrix(a.dims().rows(), a.dims().cols(), alloc);
    for (size_t i = 0; i < a.numel(); ++i)
        r.complexDataMut()[i] = op(a.complexData()[i]);
    return r;
}

} // namespace mlab
