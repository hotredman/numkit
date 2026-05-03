// libs/builtin/src/math/trig/trig_portable.cpp
//
// Reference scalar implementations of trig family: sin, cos, tan,
// sinh, cosh, tanh, asin, acos, atan, atan2, asinh, acosh, atanh,
// sind, cosd, tand, asind, acosd, atand, atan2d, sinpi, cospi.
// Compiled when NUMKIT_WITH_SIMD=OFF; the Highway-dispatched variant
// lives in trig_highway.cpp and matches this file bit-for-bit on complex
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

// ── Degree variants + sinpi/cospi (scalar fallback) ──────────────────

namespace {
constexpr double kDeg2Rad_p = 3.14159265358979323846 / 180.0;
constexpr double kRad2Deg_p = 180.0 / 3.14159265358979323846;
constexpr double kPi_p      = 3.14159265358979323846;

inline double sind_scalar_p(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = std::fmod(x, 360.0);
    if (xr == 0.0 || xr == 180.0 || xr == -180.0) return 0.0;
    if (xr == 90.0)  return  1.0;
    if (xr == -90.0) return -1.0;
    return std::sin(xr * kDeg2Rad_p);
}
inline double cosd_scalar_p(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = std::fmod(x, 360.0);
    if (xr == 90.0 || xr == -90.0 || xr == 270.0 || xr == -270.0) return 0.0;
    if (xr == 0.0) return  1.0;
    if (xr == 180.0 || xr == -180.0) return -1.0;
    return std::cos(xr * kDeg2Rad_p);
}
inline double tand_scalar_p(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = std::fmod(x, 360.0);
    if (xr == 0.0 || xr == 180.0 || xr == -180.0) return 0.0;
    if (xr == 90.0)  return  std::numeric_limits<double>::infinity();
    if (xr == -90.0) return -std::numeric_limits<double>::infinity();
    if (xr == 270.0) return  std::numeric_limits<double>::infinity();
    if (xr == -270.0)return -std::numeric_limits<double>::infinity();
    return std::tan(xr * kDeg2Rad_p);
}
inline double sinpi_scalar_p(double x)
{
    if (std::isnan(x)) return x;
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = std::remainder(x, 2.0);
    if (xr == 0.0 || xr == 1.0 || xr == -1.0) return 0.0;
    if (xr ==  0.5) return  1.0;
    if (xr == -0.5) return -1.0;
    return std::sin(kPi_p * xr);
}
inline double cospi_scalar_p(double x)
{
    if (std::isnan(x)) return x;
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = std::remainder(x, 2.0);
    if (xr ==  0.5 || xr == -0.5) return 0.0;
    if (xr ==  0.0) return  1.0;
    if (xr ==  1.0 || xr == -1.0) return -1.0;
    return std::cos(kPi_p * xr);
}
} // anonymous

Value sind(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sin(c * kDeg2Rad_p); }, mr);
    return unaryDouble(x, [](double v) { return sind_scalar_p(v); }, mr);
}
Value cosd(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cos(c * kDeg2Rad_p); }, mr);
    return unaryDouble(x, [](double v) { return cosd_scalar_p(v); }, mr);
}
Value tand(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::tan(c * kDeg2Rad_p); }, mr);
    return unaryDouble(x, [](double v) { return tand_scalar_p(v); }, mr);
}
Value asind(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asin(c) * kRad2Deg_p; }, mr);
    return unaryDouble(x, [](double v) { return std::asin(v) * kRad2Deg_p; }, mr);
}
Value acosd(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acos(c) * kRad2Deg_p; }, mr);
    return unaryDouble(x, [](double v) { return std::acos(v) * kRad2Deg_p; }, mr);
}
Value atand(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atan(c) * kRad2Deg_p; }, mr);
    return unaryDouble(x, [](double v) { return std::atan(v) * kRad2Deg_p; }, mr);
}
Value atan2d(std::pmr::memory_resource *mr, const Value &y, const Value &x)
{
    return elementwiseDouble(y, x, [](double yy, double xx) { return std::atan2(yy, xx) * kRad2Deg_p; }, mr);
}
Value sinpi(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sin(kPi_p * c); }, mr);
    return unaryDouble(x, [](double v) { return sinpi_scalar_p(v); }, mr);
}
Value cospi(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cos(kPi_p * c); }, mr);
    return unaryDouble(x, [](double v) { return cospi_scalar_p(v); }, mr);
}

} // namespace numkit::builtin
