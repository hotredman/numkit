// libs/builtin/src/math/elementary/backends/transcendental_portable.cpp
//
// Reference scalar implementations of sin / cos / exp / log. Compiled
// when NUMKIT_WITH_SIMD=OFF; the Highway-dispatched variant lives in
// transcendental_simd.cpp and matches this file bit-for-bit on
// complex inputs (SIMD only helps the real-vector fast path).

#include <numkit/builtin/math/elementary/exponents.hpp>
#include <numkit/builtin/math/elementary/trigonometry.hpp>

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
Value unaryRealDoubleHint(Allocator &alloc, const Value &x, Value *hint,
                           ScalarOp scalarOp, ComplexOp complexOp)
{
    if (x.isComplex())
        return unaryComplex(x, complexOp, &alloc);
    if (x.isScalar())
        return Value::scalar(scalarOp(x.toScalar()), &alloc);

    if (hint && hint->isHeapDouble() && hint->heapRefCount() == 1
        && hint->dims() == x.dims()) {
        Value r = std::move(*hint);
        const double *in  = x.doubleData();
        double       *out = r.doubleDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            out[i] = scalarOp(in[i]);
        return r;
    }
    return unaryDouble(x, scalarOp, &alloc);
}

} // namespace

Value sin(Allocator &alloc, const Value &x, Value *hint)
{
    return unaryRealDoubleHint(alloc, x, hint,
        [](double v) { return std::sin(v); },
        [](const Complex &c) { return std::sin(c); });
}

Value cos(Allocator &alloc, const Value &x, Value *hint)
{
    return unaryRealDoubleHint(alloc, x, hint,
        [](double v) { return std::cos(v); },
        [](const Complex &c) { return std::cos(c); });
}

Value exp(Allocator &alloc, const Value &x, Value *hint)
{
    return unaryRealDoubleHint(alloc, x, hint,
        [](double v) { return std::exp(v); },
        [](const Complex &c) { return std::exp(c); });
}

Value log(Allocator &alloc, const Value &x, Value *hint)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::log(c); }, &alloc);
    if (x.isScalar() && x.toScalar() < 0)
        return Value::complexScalar(std::log(Complex(x.toScalar(), 0.0)), &alloc);
    return unaryRealDoubleHint(alloc, x, hint,
        [](double v) { return std::log(v); },
        [](const Complex &c) { return std::log(c); });
}

} // namespace numkit::builtin
