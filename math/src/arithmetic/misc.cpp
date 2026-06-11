// toolboxes/builtin/src/math/elementary/misc.cpp
//
// Miscellaneous elementary-math builtins: deg2rad, rad2deg, mod, rem,
// hypot, nthroot.

#include <numkit/math/arithmetic/misc.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"
#include "mod_simd.hpp"

#include <cmath>

namespace numkit::math {

Value deg2rad(const Value &x, std::pmr::memory_resource *mr)
{
    constexpr double k = 3.14159265358979323846 / 180.0;
    return unaryDouble(x, [k](double v) { return v * k; }, mr);
}

Value rad2deg(const Value &x, std::pmr::memory_resource *mr)
{
    constexpr double k = 180.0 / 3.14159265358979323846;
    return unaryDouble(x, [k](double v) { return v * k; }, mr);
}

// Angle-wrapping family (wrapToPi / wrapTo2Pi / wrapTo180 / wrapTo360).
// These match MATLAB's Mapping-Toolbox definitions exactly, including the
// "positive input that lands on the open boundary keeps the upper bound"
// quirk: wrapTo2Pi(2*pi)=2*pi (not 0), but wrapTo2Pi(0)=0 and
// wrapTo2Pi(-2*pi)=0; likewise wrapTo360(360)=360 but wrapTo360(0)=0.
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// MATLAB-style modulo: result has the sign of the divisor (always >= 0
// here since the period is positive).
inline double wrapMod(double x, double period)
{
    return x - std::floor(x / period) * period;
}

// wrap to [0, period): positive inputs landing exactly on 0 snap to period.
inline double wrapToUpper(double x, double period)
{
    const bool positiveInput = x > 0.0;
    double m = wrapMod(x, period);
    if (m == 0.0 && positiveInput) m = period;
    return m;
}

// wrap to [-half, half]: only values strictly outside are wrapped, so the
// closed endpoints (-half, +half) are preserved.
inline double wrapToSym(double x, double half, double period)
{
    if (x < -half || half < x) return wrapToUpper(x + half, period) - half;
    return x;
}
} // namespace

Value wrapToPi(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return wrapToSym(v, kPi, kTwoPi); }, mr);
}

Value wrapTo2Pi(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return wrapToUpper(v, kTwoPi); }, mr);
}

Value wrapTo180(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return wrapToSym(v, 180.0, 360.0); }, mr);
}

Value wrapTo360(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return wrapToUpper(v, 360.0); }, mr);
}

// mod/rem share class handling: if either operand is integer-typed, MATLAB
// keeps that integer class on the result (mod(int8(7),int8(3))=1 int8; a
// double operand is promoted to the integer class). elementwiseDouble's array
// path reads doubleData(), which throws on integer ARRAYS, so integer operands
// are first promoted to double; the result is cast back (values are already in
// range, so the cast is exact). The all-double path is untouched.
namespace {
template <typename Op>
Value modRemImpl(const Value &a, const Value &b, Op op,
                 std::pmr::memory_resource *mr)
{
    const bool ai = isIntegerType(a.type());
    const bool bi = isIntegerType(b.type());
    if (!ai && !bi)
        return elementwiseDouble(a, b, op, mr);
    Value ad = ai ? toDoubleValue(a, mr) : a;
    Value bd = bi ? toDoubleValue(b, mr) : b;
    Value r = elementwiseDouble(ad, bd, op, mr);
    return doubleToIntegerExact(r, ai ? a.type() : b.type(), mr);
}
} // namespace

Value mod(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    // SIMD fast path for the hot real-double cases: same-shape arrays,
    // array-by-scalar, scalar-by-array. Integer types, complex, empties,
    // scalar-scalar and broadcasting fall through to the scalar reference
    // (modRemImpl), which the SIMD loops are bit-identical to.
    if (a.type() == ValueType::DOUBLE && b.type() == ValueType::DOUBLE
        && !a.isEmpty() && !b.isEmpty()) {
        const bool aScalar = a.isScalar();
        const bool bScalar = b.isScalar();
        if (!aScalar && !bScalar && a.dims() == b.dims()) {
            Value r = createLike(a, ValueType::DOUBLE, mr);
            detail::modLoopVV(a.doubleData(), b.doubleData(), r.doubleDataMut(), a.numel());
            return r;
        }
        if (!aScalar && bScalar) {
            Value r = createLike(a, ValueType::DOUBLE, mr);
            detail::modLoopVS(a.doubleData(), b.toScalar(), r.doubleDataMut(), a.numel());
            return r;
        }
        if (aScalar && !bScalar) {
            Value r = createLike(b, ValueType::DOUBLE, mr);
            detail::modLoopSV(a.toScalar(), b.doubleData(), r.doubleDataMut(), b.numel());
            return r;
        }
    }
    return modRemImpl(a, b,
                      [](double aa, double bb) {
                          return bb != 0 ? aa - std::floor(aa / bb) * bb : aa;
                      },
                      mr);
}

Value rem(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return modRemImpl(a, b,
                      [](double aa, double bb) { return std::fmod(aa, bb); }, mr);
}

// hypot moved to math/trig/trig_highway.cpp (composes Sqrt(a²+b²) via
// SIMD; pairs naturally with atan2 which lives there). The portable
// scalar version is in trig_portable.cpp.

// nthroot(x, n): real n-th root. For negative x with odd integer n,
// returns the negative real root (sign(x) * |x|^(1/n)). For negative x
// with non-odd n, returns NaN.
Value nthroot(const Value &x, const Value &n, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(x, n, [](double xv, double nv) {
        if (nv == 0.0) return std::nan("");
        if (xv >= 0.0) return std::pow(xv, 1.0 / nv);
        const double rounded = std::round(nv);
        if (rounded != nv) return std::nan("");
        const long long ni = static_cast<long long>(rounded);
        if (ni % 2 == 0) return std::nan("");
        return -std::pow(-xv, 1.0 / nv);
    }, mr);
}

} // namespace numkit::math
