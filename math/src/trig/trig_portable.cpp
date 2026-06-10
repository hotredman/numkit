// toolboxes/builtin/src/math/trig/trig_portable.cpp
//
// Reference scalar implementations of trig family: sin, cos, tan,
// sinh, cosh, tanh, asin, acos, atan, atan2, asinh, acosh, atanh,
// sind, cosd, tand, asind, acosd, atand, atan2d, sinpi, cospi.
// Compiled when NUMKIT_WITH_SIMD=OFF; the Highway-dispatched variant
// lives in trig_highway.cpp and matches this file bit-for-bit on complex
// inputs (SIMD only helps the real-vector fast path).

#include <numkit/builtin/math/trig/trigonometry.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"
#include "sinpi_kernel.hpp"

#include <cmath>
#include <complex>

namespace numkit::math {

namespace {

// Shared scaffolding: complex / scalar shortcuts, then either reuse
// `*hint` (uniquely-owned heap double of matching shape) or allocate
// a fresh result and apply ScalarOp element-wise. See the docblock
// on abs() in math/elementary/misc.hpp for the full hint contract.
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

} // namespace

Value sin(const Value &x, Value *hint, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleHint(x, hint, [](double v) { return std::sin(v); }, [](const Complex &c) { return std::sin(c); }, mr);
}

Value cos(const Value &x, Value *hint, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleHint(x, hint, [](double v) { return std::cos(v); }, [](const Complex &c) { return std::cos(c); }, mr);
}

// ── Hyperbolic + inverse trig (scalar fallback) ──────────────────────

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

// asin/acos of a REAL argument outside [-1,1] go complex; if ANY element is
// out of range the WHOLE array is promoted (MATLAB). std::acos/std::asin on
// the complex axis use a branch cut whose imaginary SIGN disagrees with MATLAB
// on [1,+inf), so compute via acosh to match exactly:
//   acos(x>1)=0+i*acosh(x)  acos(x<-1)=pi-i*acosh(|x|)
//   asin(x>1)=pi/2-i*acosh(x)  asin(x<-1)=-pi/2+i*acosh(|x|)
namespace {
constexpr double kPiUnitTrig_p  = 3.14159265358979323846;
constexpr double kHalfPi_p      = 1.57079632679489661923;
bool anyOutsideUnitInterval_p(const Value &x)
{
    const std::size_t n = x.numel();
    for (std::size_t i = 0; i < n; ++i) {
        const double v = x.elemAsDouble(i);
        if (v < -1.0 || v > 1.0) return true;
    }
    return false;
}
Complex acosRealToComplex_p(double v)
{
    if (v >= -1.0 && v <= 1.0) return Complex(std::acos(v), 0.0);
    if (v > 1.0)               return Complex(0.0, std::acosh(v));
    return Complex(kPiUnitTrig_p, -std::acosh(-v));
}
Complex asinRealToComplex_p(double v)
{
    if (v >= -1.0 && v <= 1.0) return Complex(std::asin(v), 0.0);
    if (v > 1.0)               return Complex(kHalfPi_p, -std::acosh(v));
    return Complex(-kHalfPi_p, std::acosh(-v));
}
// MATLAB atanh of real |x|>1: atanh(1/x) + i*sign(x)*pi/2 (std::atanh flips
// the imaginary sign for x<-1).
Complex atanhRealToComplex_p(double v)
{
    if (std::isnan(v))         return Complex(v, 0.0);
    if (v >= -1.0 && v <= 1.0) return Complex(std::atanh(v), 0.0);
    return Complex(std::atanh(1.0 / v), v > 0.0 ? kHalfPi_p : -kHalfPi_p);
}
bool anyLessThanOne_p(const Value &x)
{
    const std::size_t n = x.numel();
    for (std::size_t i = 0; i < n; ++i)
        if (x.elemAsDouble(i) < 1.0) return true;
    return false;
}
Value mapRealToComplexUnit_p(const Value &x, Complex (*fn)(double),
                             std::pmr::memory_resource *mr)
{
    Value cx = x; cx.promoteToComplex(mr);
    Complex *d = cx.complexDataMut();
    const std::size_t n = cx.numel();
    for (std::size_t i = 0; i < n; ++i) d[i] = fn(d[i].real());
    return cx;
}
} // namespace

Value asin(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asin(c); }, mr);
    if (anyOutsideUnitInterval_p(x))
        return mapRealToComplexUnit_p(x, asinRealToComplex_p, mr);
    return unaryDouble(x, [](double v) { return std::asin(v); }, mr);
}

Value acos(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acos(c); }, mr);
    if (anyOutsideUnitInterval_p(x))
        return mapRealToComplexUnit_p(x, acosRealToComplex_p, mr);
    return unaryDouble(x, [](double v) { return std::acos(v); }, mr);
}

Value atan(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atan(c); }, mr);
    return unaryDouble(x, [](double v) { return std::atan(v); }, mr);
}

Value asinh(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asinh(c); }, mr);
    return unaryDouble(x, [](double v) { return std::asinh(v); }, mr);
}

Value atanh(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atanh(c); }, mr);
    if (anyOutsideUnitInterval_p(x))
        return mapRealToComplexUnit_p(x, atanhRealToComplex_p, mr);
    return unaryDouble(x, [](double v) { return std::atanh(v); }, mr);
}

Value atan2(const Value &y, const Value &x, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(y, x, [](double yy, double xx) { return std::atan2(yy, xx); }, mr);
}

Value tan(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::tan(c); }, mr);
    return unaryDouble(x, [](double v) { return std::tan(v); }, mr);
}

Value acosh(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acosh(c); }, mr);
    if (anyLessThanOne_p(x)) {
        Value cx = x; cx.promoteToComplex(mr);
        return unaryComplex(cx, [](const Complex &c) { return std::acosh(c); }, mr);
    }
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
// Accurate sin(pi*x) / cos(pi*x) via the shared kernel (exact octant
// reduction + SLEEF sinpik/cospik polynomial; see sinpi_kernel.hpp).
inline double sinpi_scalar_p(double x) { return detail::sinpi_kernel(x); }
inline double cospi_scalar_p(double x) { return detail::cospi_kernel(x); }
} // anonymous

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
    return unaryDouble(x, [](double v) { return std::asin(v) * kRad2Deg_p; }, mr);
}
Value acosd(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acos(c) * kRad2Deg_p; }, mr);
    return unaryDouble(x, [](double v) { return std::acos(v) * kRad2Deg_p; }, mr);
}
Value atand(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atan(c) * kRad2Deg_p; }, mr);
    return unaryDouble(x, [](double v) { return std::atan(v) * kRad2Deg_p; }, mr);
}
Value atan2d(const Value &y, const Value &x, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(y, x, [](double yy, double xx) { return std::atan2(yy, xx) * kRad2Deg_p; }, mr);
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

Value hypot(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return elementwiseDouble(a, b, [](double aa, double bb) { return std::hypot(aa, bb); }, mr);
}

} // namespace numkit::math
