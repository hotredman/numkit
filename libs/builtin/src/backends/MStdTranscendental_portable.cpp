// libs/builtin/src/backends/MStdTranscendental_portable.cpp
//
// Reference scalar implementations of sin / cos / exp / log. Compiled
// when NUMKIT_WITH_SIMD=OFF; the Highway-dispatched variant lives in
// MStdTranscendental_simd.cpp and matches this file bit-for-bit on
// complex inputs (SIMD only helps the real-vector fast path).

#include <numkit/m/builtin/MStdMath.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "../MStdHelpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::m::builtin {

MValue sin(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sin(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::sin(v); }, &alloc);
}

MValue cos(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cos(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::cos(v); }, &alloc);
}

MValue exp(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::exp(c); }, &alloc);
    return unaryDouble(x, [](double v) { return std::exp(v); }, &alloc);
}

MValue log(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::log(c); }, &alloc);
    if (x.isScalar() && x.toScalar() < 0)
        return MValue::complexScalar(std::log(Complex(x.toScalar(), 0.0)), &alloc);
    return unaryDouble(x, [](double v) { return std::log(v); }, &alloc);
}

} // namespace numkit::m::builtin
