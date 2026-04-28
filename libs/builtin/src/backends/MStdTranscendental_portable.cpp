// libs/builtin/src/backends/MStdTranscendental_portable.cpp
//
// Reference scalar implementations of sin / cos / exp / log. Compiled
// when NUMKIT_WITH_SIMD=OFF; the Highway-dispatched variant lives in
// MStdTranscendental_simd.cpp and matches this file bit-for-bit on
// complex inputs (SIMD only helps the real-vector fast path).

#include <numkit/m/builtin/math/elementary/exponents.hpp>
#include <numkit/m/builtin/math/elementary/trigonometry.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "../MStdHelpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::m::builtin {

namespace {

// Shared scaffolding: complex / scalar shortcuts, then either reuse
// `*hint` (uniquely-owned heap double of matching shape) or allocate
// a fresh result and apply ScalarOp element-wise. See the docblock
// on abs() in MStdMath.hpp for the full hint contract.
template <typename ScalarOp, typename ComplexOp>
MValue unaryRealDoubleHint(Allocator &alloc, const MValue &x, MValue *hint,
                           ScalarOp scalarOp, ComplexOp complexOp)
{
    if (x.isComplex())
        return unaryComplex(x, complexOp, &alloc);
    if (x.isScalar())
        return MValue::scalar(scalarOp(x.toScalar()), &alloc);

    if (hint && hint->isHeapDouble() && hint->heapRefCount() == 1
        && hint->dims() == x.dims()) {
        MValue r = std::move(*hint);
        const double *in  = x.doubleData();
        double       *out = r.doubleDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            out[i] = scalarOp(in[i]);
        return r;
    }
    return unaryDouble(x, scalarOp, &alloc);
}

} // namespace

MValue sin(Allocator &alloc, const MValue &x, MValue *hint)
{
    return unaryRealDoubleHint(alloc, x, hint,
        [](double v) { return std::sin(v); },
        [](const Complex &c) { return std::sin(c); });
}

MValue cos(Allocator &alloc, const MValue &x, MValue *hint)
{
    return unaryRealDoubleHint(alloc, x, hint,
        [](double v) { return std::cos(v); },
        [](const Complex &c) { return std::cos(c); });
}

MValue exp(Allocator &alloc, const MValue &x, MValue *hint)
{
    return unaryRealDoubleHint(alloc, x, hint,
        [](double v) { return std::exp(v); },
        [](const Complex &c) { return std::exp(c); });
}

MValue log(Allocator &alloc, const MValue &x, MValue *hint)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::log(c); }, &alloc);
    if (x.isScalar() && x.toScalar() < 0)
        return MValue::complexScalar(std::log(Complex(x.toScalar(), 0.0)), &alloc);
    return unaryRealDoubleHint(alloc, x, hint,
        [](double v) { return std::log(v); },
        [](const Complex &c) { return std::log(c); });
}

} // namespace numkit::m::builtin
