// src/builtin/src/elfun/trig_portable.cpp
//
// Reference scalar implementations of trig family for numkit::builtin.

#include <numkit/builtin/elfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/ops/helpers.hpp>
#include "sinpi_kernel.hpp"

#include <cmath>
#include <complex>

namespace numkit::builtin {

namespace {

template <typename ScalarOp, typename ComplexOp>
Value unaryRealDoubleHint(const Value &x, Value *hint, ScalarOp scalarOp, ComplexOp complexOp, std::pmr::memory_resource *mr)
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

constexpr double kDeg2Rad_p = 3.14159265358979323846 / 180.0;
constexpr double kRad2Deg_p = 180.0 / 3.14159265358979323846;
constexpr double kPi_p      = 3.14159265358979323846;

inline double reduceDeg360_p(double x) { return std::fmod(x, 360.0); }

inline double sind_scalar_p(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = reduceDeg360_p(x);
    if (xr == 0.0 || xr == 180.0 || xr == -180.0) return 0.0;
    if (xr == 90.0)  return  1.0;
    if (xr == -90.0) return -1.0;
    return std::sin(xr * kDeg2Rad_p);
}

inline double cosd_scalar_p(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = reduceDeg360_p(x);
    if (xr == 90.0 || xr == -90.0 || xr == 270.0 || xr == -270.0) return 0.0;
    if (xr == 0.0) return  1.0;
    if (xr == 180.0 || xr == -180.0) return -1.0;
    return std::cos(xr * kDeg2Rad_p);
}

inline double tand_scalar_p(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = reduceDeg360_p(x);
    if (xr == 0.0 || xr == 180.0 || xr == -180.0) return 0.0;
    if (xr == 90.0)  return  std::numeric_limits<double>::infinity();
    if (xr == -90.0) return -std::numeric_limits<double>::infinity();
    if (xr == 270.0) return  std::numeric_limits<double>::infinity();
    if (xr == -270.0)return -std::numeric_limits<double>::infinity();
    return std::tan(xr * kDeg2Rad_p);
}

inline double asind_scalar_p(double x)
{
    if (x < -1.0 || x > 1.0) return std::numeric_limits<double>::quiet_NaN();
    if (x ==  1.0) return  90.0;
    if (x == -1.0) return -90.0;
    if (x ==  0.0) return   0.0;
    return std::asin(x) * kRad2Deg_p;
}

inline double acosd_scalar_p(double x)
{
    if (x < -1.0 || x > 1.0) return std::numeric_limits<double>::quiet_NaN();
    if (x ==  1.0) return   0.0;
    if (x ==  0.0) return  90.0;
    if (x == -1.0) return 180.0;
    return std::acos(x) * kRad2Deg_p;
}

inline double atand_scalar_p(double x)
{
    if (std::isnan(x)) return x;
    if (x == 0.0) return 0.0;
    return std::atan(x) * kRad2Deg_p;
}

inline double atan2d_scalar_p(double y, double x)
{
    return std::atan2(y, x) * kRad2Deg_p;
}

inline double sinpi_scalar_p(double x) { return detail::sinpi_kernel(x); }
inline double cospi_scalar_p(double x) { return detail::cospi_kernel(x); }

} // namespace

Value sin(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleHint(x, nullptr, [](double v) { return std::sin(v); }, [](const Complex &c) { return std::sin(c); }, mr);
}

Value cos(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleHint(x, nullptr, [](double v) { return std::cos(v); }, [](const Complex &c) { return std::cos(c); }, mr);
}

Value tan(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::tan(c); }, mr);
    return unaryDouble(x, [](double v) { return std::tan(v); }, mr);
}

Value sinh(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sinh(c); }, mr);
    return unaryDouble(x, [](double v) { return std::sinh(v); }, mr);
}

Value cosh(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cosh(c); }, mr);
    return unaryDouble(x, [](double v) { return std::cosh(v); }, mr);
}

Value tanh(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::tanh(c); }, mr);
    return unaryDouble(x, [](double v) { return std::tanh(v); }, mr);
}

Value asin(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asin(c); }, mr);
    // Real out-of-domain generates complex result in MATLAB semantics
    for (size_t i = 0; i < x.numel(); ++i) {
        double v = x.elemAsDouble(i);
        if (v < -1.0 || v > 1.0) {
            return unaryComplex(x, [](const Complex &c) { return std::asin(c); }, mr);
        }
    }
    return unaryDouble(x, [](double v) { return std::asin(v); }, mr);
}

Value acos(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acos(c); }, mr);
    for (size_t i = 0; i < x.numel(); ++i) {
        double v = x.elemAsDouble(i);
        if (v < -1.0 || v > 1.0) {
            return unaryComplex(x, [](const Complex &c) { return std::acos(c); }, mr);
        }
    }
    return unaryDouble(x, [](double v) { return std::acos(v); }, mr);
}

Value atan(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atan(c); }, mr);
    return unaryDouble(x, [](double v) { return std::atan(v); }, mr);
}

Value atan2(const Value &y, const Value &x, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(y, x, [](double yy, double xx) { return std::atan2(yy, xx); }, mr);
}

Value asinh(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asinh(c); }, mr);
    return unaryDouble(x, [](double v) { return std::asinh(v); }, mr);
}

Value acosh(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acosh(c); }, mr);
    for (size_t i = 0; i < x.numel(); ++i) {
        if (x.elemAsDouble(i) < 1.0) {
            return unaryComplex(x, [](const Complex &c) { return std::acosh(c); }, mr);
        }
    }
    return unaryDouble(x, [](double v) { return std::acosh(v); }, mr);
}

Value atanh(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atanh(c); }, mr);
    for (size_t i = 0; i < x.numel(); ++i) {
        double v = x.elemAsDouble(i);
        if (v < -1.0 || v > 1.0) {
            return unaryComplex(x, [](const Complex &c) { return std::atanh(c); }, mr);
        }
    }
    return unaryDouble(x, [](double v) { return std::atanh(v); }, mr);
}

Value sind(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sin(c * kDeg2Rad_p); }, mr);
    return unaryDouble(x, [](double v) { return sind_scalar_p(v); }, mr);
}

Value cosd(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cos(c * kDeg2Rad_p); }, mr);
    return unaryDouble(x, [](double v) { return cosd_scalar_p(v); }, mr);
}

Value tand(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::tan(c * kDeg2Rad_p); }, mr);
    return unaryDouble(x, [](double v) { return tand_scalar_p(v); }, mr);
}

Value asind(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asin(c) * kRad2Deg_p; }, mr);
    return unaryDouble(x, [](double v) { return asind_scalar_p(v); }, mr);
}

Value acosd(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acos(c) * kRad2Deg_p; }, mr);
    return unaryDouble(x, [](double v) { return acosd_scalar_p(v); }, mr);
}

Value atand(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atan(c) * kRad2Deg_p; }, mr);
    return unaryDouble(x, [](double v) { return atand_scalar_p(v); }, mr);
}

Value atan2d(const Value &y, const Value &x, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(y, x, [](double yy, double xx) { return atan2d_scalar_p(yy, xx); }, mr);
}

Value sinpi(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sin(kPi_p * c); }, mr);
    return unaryDouble(x, [](double v) { return sinpi_scalar_p(v); }, mr);
}

Value cospi(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cos(kPi_p * c); }, mr);
    return unaryDouble(x, [](double v) { return cospi_scalar_p(v); }, mr);
}

} // namespace numkit::builtin
