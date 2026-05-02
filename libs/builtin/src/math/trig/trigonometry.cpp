// libs/builtin/src/math/elementary/trigonometry.cpp
//
// tan / asin / acos / atan / atan2. sin / cos live in
// libs/builtin/src/backends/MStdTranscendental_*.cpp (SIMD-backed).

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/trig/trigonometry.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::builtin {

Value tan(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::tan(c); }, mr);
    return unaryDouble(x, [](double v) { return std::tan(v); }, mr);
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

Value atan2(std::pmr::memory_resource *mr, const Value &y, const Value &x)
{
    return elementwiseDouble(y, x, [](double yy, double xx) { return std::atan2(yy, xx); }, mr);
}

// ── Hyperbolic ────────────────────────────────────────────────────────

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

Value asinh(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asinh(c); }, mr);
    return unaryDouble(x, [](double v) { return std::asinh(v); }, mr);
}

Value acosh(std::pmr::memory_resource *mr, const Value &x)
{
    // Real branch returns NaN for |x|<1, where MATLAB returns a complex
    // value. Promote to complex when we hit any out-of-domain element.
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acosh(c); }, mr);
    if (x.isScalar() && x.toScalar() < 1.0)
        return Value::complexScalar(std::acosh(Complex(x.toScalar(), 0.0)), mr);
    return unaryDouble(x, [](double v) { return std::acosh(v); }, mr);
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

// ── Degree-input/-output trig ─────────────────────────────────────────
//
// MATLAB exposes sind/cosd/tand etc. that take/return degrees. Exact
// zeros at integer multiples of 180° (sind) and exact ±1 at integer
// multiples of 90° (cosd/sind) are preserved by snapping the reduced
// argument before the libm call.

namespace {
constexpr double kDeg2Rad = 3.14159265358979323846 / 180.0;
constexpr double kRad2Deg = 180.0 / 3.14159265358979323846;
constexpr double kPi      = 3.14159265358979323846;

// Reduce x (in degrees) to (-360, 360) without losing exact integer
// multiples of 90°.
inline double reduceDeg360(double x) { return std::fmod(x, 360.0); }

inline double sind_scalar(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = reduceDeg360(x);
    if (xr == 0.0 || xr == 180.0 || xr == -180.0) return 0.0;
    if (xr == 90.0)  return  1.0;
    if (xr == -90.0) return -1.0;
    return std::sin(xr * kDeg2Rad);
}

inline double cosd_scalar(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = reduceDeg360(x);
    if (xr == 90.0 || xr == -90.0 || xr == 270.0 || xr == -270.0) return 0.0;
    if (xr == 0.0) return  1.0;
    if (xr == 180.0 || xr == -180.0) return -1.0;
    return std::cos(xr * kDeg2Rad);
}

inline double tand_scalar(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = reduceDeg360(x);
    if (xr == 0.0 || xr == 180.0 || xr == -180.0) return 0.0;
    // ±90°, ±270° → ±Inf in MATLAB.
    if (xr == 90.0)  return  std::numeric_limits<double>::infinity();
    if (xr == -90.0) return -std::numeric_limits<double>::infinity();
    if (xr == 270.0) return  std::numeric_limits<double>::infinity();
    if (xr == -270.0)return -std::numeric_limits<double>::infinity();
    return std::tan(xr * kDeg2Rad);
}

inline double sinpi_scalar(double x)
{
    if (std::isnan(x)) return x;
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    // remainder reduces to [-1, 1] (the period of sin(pi*x) is 2).
    const double xr = std::remainder(x, 2.0);
    if (xr == 0.0 || xr == 1.0 || xr == -1.0) return 0.0;
    if (xr ==  0.5) return  1.0;
    if (xr == -0.5) return -1.0;
    return std::sin(kPi * xr);
}

inline double cospi_scalar(double x)
{
    if (std::isnan(x)) return x;
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = std::remainder(x, 2.0);
    if (xr ==  0.5 || xr == -0.5) return 0.0;
    if (xr ==  0.0) return  1.0;
    if (xr ==  1.0 || xr == -1.0) return -1.0;
    return std::cos(kPi * xr);
}
} // anonymous

Value sind(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sin(c * kDeg2Rad); }, mr);
    return unaryDouble(x, [](double v) { return sind_scalar(v); }, mr);
}

Value cosd(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cos(c * kDeg2Rad); }, mr);
    return unaryDouble(x, [](double v) { return cosd_scalar(v); }, mr);
}

Value tand(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::tan(c * kDeg2Rad); }, mr);
    return unaryDouble(x, [](double v) { return tand_scalar(v); }, mr);
}

Value asind(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asin(c) * kRad2Deg; }, mr);
    return unaryDouble(x, [](double v) { return std::asin(v) * kRad2Deg; }, mr);
}

Value acosd(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acos(c) * kRad2Deg; }, mr);
    return unaryDouble(x, [](double v) { return std::acos(v) * kRad2Deg; }, mr);
}

Value atand(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atan(c) * kRad2Deg; }, mr);
    return unaryDouble(x, [](double v) { return std::atan(v) * kRad2Deg; }, mr);
}

Value atan2d(std::pmr::memory_resource *mr, const Value &y, const Value &x)
{
    return elementwiseDouble(y, x,
        [](double yy, double xx) { return std::atan2(yy, xx) * kRad2Deg; }, mr);
}

// ── Pi-scaled ─────────────────────────────────────────────────────────

Value sinpi(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sin(kPi * c); }, mr);
    return unaryDouble(x, [](double v) { return sinpi_scalar(v); }, mr);
}

Value cospi(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cos(kPi * c); }, mr);
    return unaryDouble(x, [](double v) { return cospi_scalar(v); }, mr);
}

// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {

#define NK_UNARY_ADAPTER(name, fn)                                              \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "m:" #name ":nargin");                 \
        outs[0] = fn(ctx.engine->resource(), args[0]);                          \
    }

NK_UNARY_ADAPTER(tan,  tan)
NK_UNARY_ADAPTER(asin, asin)
NK_UNARY_ADAPTER(acos, acos)
NK_UNARY_ADAPTER(atan, atan)

NK_UNARY_ADAPTER(sinh,  sinh)
NK_UNARY_ADAPTER(cosh,  cosh)
NK_UNARY_ADAPTER(tanh,  tanh)
NK_UNARY_ADAPTER(asinh, asinh)
NK_UNARY_ADAPTER(acosh, acosh)
NK_UNARY_ADAPTER(atanh, atanh)

NK_UNARY_ADAPTER(sind,  sind)
NK_UNARY_ADAPTER(cosd,  cosd)
NK_UNARY_ADAPTER(tand,  tand)
NK_UNARY_ADAPTER(asind, asind)
NK_UNARY_ADAPTER(acosd, acosd)
NK_UNARY_ADAPTER(atand, atand)

NK_UNARY_ADAPTER(sinpi, sinpi)
NK_UNARY_ADAPTER(cospi, cospi)

#undef NK_UNARY_ADAPTER

void atan2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("atan2: requires 2 arguments",
                     0, 0, "atan2", "", "m:atan2:nargin");
    outs[0] = atan2(ctx.engine->resource(), args[0], args[1]);
}

void atan2d_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("atan2d: requires 2 arguments",
                     0, 0, "atan2d", "", "m:atan2d:nargin");
    outs[0] = atan2d(ctx.engine->resource(), args[0], args[1]);
}

} // namespace detail

} // namespace numkit::builtin
