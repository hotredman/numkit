// libs/builtin/src/math/trig/trig_portable.cpp
//
// Reference scalar implementations of trig family: sin, cos, sinh,
// cosh, tanh, asin, acos, atan, atan2, asinh, atanh. Compiled when
// NUMKIT_WITH_SIMD=OFF; the Highway-dispatched variant lives in
// trig_simd.cpp and matches this file bit-for-bit on complex inputs
// (SIMD only helps the real-vector fast path).

#include <numkit/builtin/math/trig/trigonometry.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::builtin {

namespace {

// Shared scaffolding: complex / scalar shortcuts, then either reuse
// `*hint` (uniquely-owned heap double of matching shape) or allocate
// a fresh result and apply ScalarOp element-wise. See the docblock
// on abs() in math/elementary/misc.hpp for the full hint contract.
template <typename ScalarOp, typename ComplexOp>
Value unaryRealDoubleHint(std::pmr::memory_resource *mr, const Value &x, Value *hint,
                           ScalarOp scalarOp, ComplexOp complexOp)
{
    if (x.isComplex())
        return unaryComplex(x, complexOp, mr);
    if (x.isScalar())
        return Value::scalar(scalarOp(x.toScalar()), mr);

    if (hint && hint->isHeapDouble() && hint->heapRefCount() == 1
        && hint->dims() == x.dims()) {
        Value r = std::move(*hint);
        const double *in  = x.doubleData();
        double       *out = r.doubleDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            out[i] = scalarOp(in[i]);
        return r;
    }
    return unaryDouble(x, scalarOp, mr);
}

} // namespace

Value sin(std::pmr::memory_resource *mr, const Value &x, Value *hint)
{
    return unaryRealDoubleHint(mr, x, hint,
        [](double v) { return std::sin(v); },
        [](const Complex &c) { return std::sin(c); });
}

Value cos(std::pmr::memory_resource *mr, const Value &x, Value *hint)
{
    return unaryRealDoubleHint(mr, x, hint,
        [](double v) { return std::cos(v); },
        [](const Complex &c) { return std::cos(c); });
}

// ── Hyperbolic + inverse trig (scalar fallback) ──────────────────────

Value sinh(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sinh(c); }, mr);
    return unaryDouble(x, [](double v) { return std::sinh(v); }, mr);
}

Value cosh(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cosh(c); }, mr);
    return unaryDouble(x, [](double v) { return std::cosh(v); }, mr);
}

Value tanh(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::tanh(c); }, mr);
    return unaryDouble(x, [](double v) { return std::tanh(v); }, mr);
}

Value asin(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asin(c); }, mr);
    return unaryDouble(x, [](double v) { return std::asin(v); }, mr);
}

Value acos(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acos(c); }, mr);
    return unaryDouble(x, [](double v) { return std::acos(v); }, mr);
}

Value atan(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atan(c); }, mr);
    return unaryDouble(x, [](double v) { return std::atan(v); }, mr);
}

Value asinh(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asinh(c); }, mr);
    return unaryDouble(x, [](double v) { return std::asinh(v); }, mr);
}

Value atanh(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atanh(c); }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v < -1.0 || v > 1.0)
            return Value::complexScalar(std::atanh(Complex(v, 0.0)), mr);
    }
    return unaryDouble(x, [](double v) { return std::atanh(v); }, mr);
}

Value atan2(std::pmr::memory_resource *mr, const Value &y, const Value &x)
{
    return elementwiseDouble(y, x, [](double yy, double xx) { return std::atan2(yy, xx); }, mr);
}

Value tan(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::tan(c); }, mr);
    return unaryDouble(x, [](double v) { return std::tan(v); }, mr);
}

Value acosh(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acosh(c); }, mr);
    if (x.isScalar() && x.toScalar() < 1.0)
        return Value::complexScalar(std::acosh(Complex(x.toScalar(), 0.0)), mr);
    return unaryDouble(x, [](double v) { return std::acosh(v); }, mr);
}

} // namespace numkit::builtin
