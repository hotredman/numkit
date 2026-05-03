// libs/builtin/src/math/trig/trig_portable.cpp
//
// Reference scalar implementations of trig family (sin / cos for now).
// Compiled when NUMKIT_WITH_SIMD=OFF; the Highway-dispatched variant
// lives in trig_simd.cpp and matches this file bit-for-bit on complex
// inputs (SIMD only helps the real-vector fast path).

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

} // namespace numkit::builtin
