// libs/builtin/src/math/trig/trig_recip_portable.cpp
//
// Reference scalar implementations of the reciprocal trig family
// (sec / csc / cot / sech / csch / coth / secd / cscd / cotd and their
// inverses). Compiled when NUMKIT_WITH_SIMD=OFF; the Highway-dispatched
// variant lives in trig_recip_highway.cpp and matches this file
// bit-for-bit on complex / scalar inputs (SIMD only helps real-vector).

#include <numkit/builtin/math/trig/trigonometry.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::builtin {

namespace {
constexpr double kDeg2Rad = 3.14159265358979323846 / 180.0;
constexpr double kRad2Deg = 180.0 / 3.14159265358979323846;

inline double sind_scalar(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = std::fmod(x, 360.0);
    if (xr == 0.0 || xr == 180.0 || xr == -180.0) return 0.0;
    if (xr == 90.0)  return  1.0;
    if (xr == -90.0) return -1.0;
    return std::sin(xr * kDeg2Rad);
}

inline double cosd_scalar(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = std::fmod(x, 360.0);
    if (xr == 90.0 || xr == -90.0 || xr == 270.0 || xr == -270.0) return 0.0;
    if (xr == 0.0) return  1.0;
    if (xr == 180.0 || xr == -180.0) return -1.0;
    return std::cos(xr * kDeg2Rad);
}
} // namespace

// ── Forward reciprocal ─────────────────────────────────────────────

Value sec(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return Complex(1.0) / std::cos(c); }, mr);
    return unaryDouble(x, [](double v) { return 1.0 / std::cos(v); }, mr);
}

Value csc(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return Complex(1.0) / std::sin(c); }, mr);
    return unaryDouble(x, [](double v) { return 1.0 / std::sin(v); }, mr);
}

Value cot(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cos(c) / std::sin(c); }, mr);
    return unaryDouble(x, [](double v) { return std::cos(v) / std::sin(v); }, mr);
}

// ── Hyperbolic reciprocal ──────────────────────────────────────────

Value sech(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return Complex(1.0) / std::cosh(c); }, mr);
    return unaryDouble(x, [](double v) { return 1.0 / std::cosh(v); }, mr);
}

Value csch(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return Complex(1.0) / std::sinh(c); }, mr);
    return unaryDouble(x, [](double v) { return 1.0 / std::sinh(v); }, mr);
}

Value coth(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cosh(c) / std::sinh(c); }, mr);
    return unaryDouble(x, [](double v) { return std::cosh(v) / std::sinh(v); }, mr);
}

// ── Degree reciprocal ──────────────────────────────────────────────

Value secd(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return Complex(1.0) / std::cos(c * kDeg2Rad); }, mr);
    return unaryDouble(x, [](double v) { return 1.0 / cosd_scalar(v); }, mr);
}

Value cscd(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return Complex(1.0) / std::sin(c * kDeg2Rad); }, mr);
    return unaryDouble(x, [](double v) { return 1.0 / sind_scalar(v); }, mr);
}

Value cotd(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) {
            return std::cos(c * kDeg2Rad) / std::sin(c * kDeg2Rad);
        }, mr);
    return unaryDouble(x, [](double v) { return cosd_scalar(v) / sind_scalar(v); }, mr);
}

// ── Inverse reciprocal ─────────────────────────────────────────────

Value asec(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acos(Complex(1.0) / c); }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v > -1.0 && v < 1.0)
            return Value::complexScalar(std::acos(Complex(1.0 / v, 0.0)), mr);
    }
    return unaryDouble(x, [](double v) { return std::acos(1.0 / v); }, mr);
}

Value acsc(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asin(Complex(1.0) / c); }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v > -1.0 && v < 1.0 && v != 0.0)
            return Value::complexScalar(std::asin(Complex(1.0 / v, 0.0)), mr);
    }
    return unaryDouble(x, [](double v) { return std::asin(1.0 / v); }, mr);
}

Value acot(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atan(Complex(1.0) / c); }, mr);
    return unaryDouble(x, [](double v) { return std::atan(1.0 / v); }, mr);
}

// ── Inverse hyperbolic reciprocal ──────────────────────────────────

Value asech(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acosh(Complex(1.0) / c); }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v <= 0.0 || v > 1.0)
            return Value::complexScalar(std::acosh(Complex(1.0 / v, 0.0)), mr);
    }
    return unaryDouble(x, [](double v) { return std::acosh(1.0 / v); }, mr);
}

Value acsch(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asinh(Complex(1.0) / c); }, mr);
    return unaryDouble(x, [](double v) { return std::asinh(1.0 / v); }, mr);
}

Value acoth(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atanh(Complex(1.0) / c); }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v >= -1.0 && v <= 1.0)
            return Value::complexScalar(std::atanh(Complex(1.0 / v, 0.0)), mr);
    }
    return unaryDouble(x, [](double v) { return std::atanh(1.0 / v); }, mr);
}

// ── Inverse degree reciprocal ──────────────────────────────────────

Value asecd(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) {
            return std::acos(Complex(1.0) / c) * kRad2Deg;
        }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v > -1.0 && v < 1.0)
            return Value::complexScalar(std::acos(Complex(1.0 / v, 0.0)) * kRad2Deg, mr);
    }
    return unaryDouble(x, [](double v) { return std::acos(1.0 / v) * kRad2Deg; }, mr);
}

Value acscd(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) {
            return std::asin(Complex(1.0) / c) * kRad2Deg;
        }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v > -1.0 && v < 1.0 && v != 0.0)
            return Value::complexScalar(std::asin(Complex(1.0 / v, 0.0)) * kRad2Deg, mr);
    }
    return unaryDouble(x, [](double v) { return std::asin(1.0 / v) * kRad2Deg; }, mr);
}

Value acotd(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) {
            return std::atan(Complex(1.0) / c) * kRad2Deg;
        }, mr);
    return unaryDouble(x, [](double v) { return std::atan(1.0 / v) * kRad2Deg; }, mr);
}

} // namespace numkit::builtin
