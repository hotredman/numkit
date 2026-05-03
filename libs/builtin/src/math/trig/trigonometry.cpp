// libs/builtin/src/math/trig/trigonometry.cpp
//
// Trig functions whose implementations don't fit the simple SIMD
// backend pattern: the degree / multiple-of-π variants. The
// SIMD-friendly trig kernels (sin, cos, tan, sinh, cosh, tanh, asin,
// acos, atan, atan2, asinh, acosh, atanh) live in trig_highway.cpp /
// trig_portable.cpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/arithmetic/misc.hpp>          // hypot decl
#include <numkit/builtin/math/trig/trigonometry.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::builtin {

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

} // anonymous

// sind / cosd / tand / asind / acosd / atand / atan2d / sinpi / cospi
// now live in trig_highway.cpp / trig_portable.cpp. The snap-helpers
// (sind_scalar, cosd_scalar, tand_scalar) remain in this file's
// anonymous namespace because secd / cscd / cotd reuse them below.

// ── Reciprocal trig (sec/csc/cot families) ────────────────────────────
//
// Defined as `1 / primary` for the forward forms and via the primary
// inverse on `1/x` for the inverse forms (matches MATLAB).
// IEEE 754 propagates ±Inf for divide-by-zero, so cot(0), csc(0),
// sech(±Inf) etc. fall out naturally without special-casing.

Value sec(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return Complex(1.0) / std::cos(c); }, mr);
    return unaryDouble(x, [](double v) { return 1.0 / std::cos(v); }, mr);
}

Value csc(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return Complex(1.0) / std::sin(c); }, mr);
    return unaryDouble(x, [](double v) { return 1.0 / std::sin(v); }, mr);
}

Value cot(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cos(c) / std::sin(c); }, mr);
    return unaryDouble(x, [](double v) { return std::cos(v) / std::sin(v); }, mr);
}

Value sech(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return Complex(1.0) / std::cosh(c); }, mr);
    return unaryDouble(x, [](double v) { return 1.0 / std::cosh(v); }, mr);
}

Value csch(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return Complex(1.0) / std::sinh(c); }, mr);
    return unaryDouble(x, [](double v) { return 1.0 / std::sinh(v); }, mr);
}

Value coth(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cosh(c) / std::sinh(c); }, mr);
    return unaryDouble(x, [](double v) { return std::cosh(v) / std::sinh(v); }, mr);
}

Value secd(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return Complex(1.0) / std::cos(c * kDeg2Rad); }, mr);
    // sec is undefined at ±90°; cosd snaps those to 0 → 1/0 = ±Inf in IEEE.
    return unaryDouble(x, [](double v) { return 1.0 / cosd_scalar(v); }, mr);
}

Value cscd(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return Complex(1.0) / std::sin(c * kDeg2Rad); }, mr);
    return unaryDouble(x, [](double v) { return 1.0 / sind_scalar(v); }, mr);
}

Value cotd(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) {
            return std::cos(c * kDeg2Rad) / std::sin(c * kDeg2Rad);
        }, mr);
    // cotd(0) = +Inf, cotd(180) = +Inf — driven by sind_scalar exact zeros.
    return unaryDouble(x, [](double v) { return cosd_scalar(v) / sind_scalar(v); }, mr);
}

// ── Inverse reciprocal trig ──────────────────────────────────────────
// Defined as the primary inverse of 1/x. acot uses atan2(1, x) so that
// acot(0) = π/2 (MATLAB convention) instead of NaN-from-1/0.

Value asec(std::pmr::memory_resource *mr, const Value &x)
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

Value acsc(std::pmr::memory_resource *mr, const Value &x)
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

Value acot(std::pmr::memory_resource *mr, const Value &x)
{
    // atan2(1, x) → x>0: atan(1/x); x<0: atan(1/x)+π/2 mapping; x=0: π/2.
    // MATLAB's acot uses the principal branch that goes to π/2 at x=0.
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atan(Complex(1.0) / c); }, mr);
    return unaryDouble(x, [](double v) { return std::atan(1.0 / v); }, mr);
}

Value asech(std::pmr::memory_resource *mr, const Value &x)
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

Value acsch(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asinh(Complex(1.0) / c); }, mr);
    return unaryDouble(x, [](double v) { return std::asinh(1.0 / v); }, mr);
}

Value acoth(std::pmr::memory_resource *mr, const Value &x)
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

Value asecd(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acos(Complex(1.0) / c) * kRad2Deg; }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v > -1.0 && v < 1.0)
            return Value::complexScalar(std::acos(Complex(1.0 / v, 0.0)) * kRad2Deg, mr);
    }
    return unaryDouble(x, [](double v) { return std::acos(1.0 / v) * kRad2Deg; }, mr);
}

Value acscd(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asin(Complex(1.0) / c) * kRad2Deg; }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v > -1.0 && v < 1.0 && v != 0.0)
            return Value::complexScalar(std::asin(Complex(1.0 / v, 0.0)) * kRad2Deg, mr);
    }
    return unaryDouble(x, [](double v) { return std::asin(1.0 / v) * kRad2Deg; }, mr);
}

Value acotd(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atan(Complex(1.0) / c) * kRad2Deg; }, mr);
    return unaryDouble(x, [](double v) { return std::atan(1.0 / v) * kRad2Deg; }, mr);
}

// ── Coordinate transforms ────────────────────────────────────────────
// MATLAB conventions:
//   cart2pol(x, y)     → theta = atan2(y, x), rho = hypot(x, y)
//   cart2pol(x, y, z)  → adds z passthrough (cylindrical)
//   pol2cart(t, r)     → x = r*cos(t),  y = r*sin(t)
//   cart2sph(x, y, z)  → az = atan2(y, x),
//                        el = atan2(z, hypot(x, y)),
//                        r  = sqrt(x²+y²+z²)
//   sph2cart(az, el, r)→ x = r*cos(el)*cos(az),
//                        y = r*cos(el)*sin(az),
//                        z = r*sin(el)

PolarPair cart2pol(std::pmr::memory_resource *mr, const Value &x, const Value &y)
{
    // Compose via the SIMD-dispatched atan2 + hypot (in trig_highway.cpp).
    // Each call gets the full vector SIMD path on real-same-shape inputs.
    Value theta = atan2(mr, y, x);
    Value rho   = hypot(mr, x, y);
    return { std::move(theta), std::move(rho) };
}

CylTriple cart2pol(std::pmr::memory_resource *mr,
                   const Value &x, const Value &y, const Value &z)
{
    auto [theta, rho] = cart2pol(mr, x, y);
    return { std::move(theta), std::move(rho), z };
}

CartPair pol2cart(std::pmr::memory_resource *mr,
                  const Value &theta, const Value &rho)
{
    Value xv = elementwiseDouble(rho, theta,
        [](double r, double t) { return r * std::cos(t); }, mr);
    Value yv = elementwiseDouble(rho, theta,
        [](double r, double t) { return r * std::sin(t); }, mr);
    return { std::move(xv), std::move(yv) };
}

CartTriple pol2cart(std::pmr::memory_resource *mr,
                    const Value &theta, const Value &rho, const Value &z)
{
    auto [xv, yv] = pol2cart(mr, theta, rho);
    return { std::move(xv), std::move(yv), z };
}

SphTriple cart2sph(std::pmr::memory_resource *mr,
                   const Value &x, const Value &y, const Value &z)
{
    Value az = elementwiseDouble(y, x,
        [](double yy, double xx) { return std::atan2(yy, xx); }, mr);
    Value rxy = elementwiseDouble(x, y,
        [](double xx, double yy) { return std::hypot(xx, yy); }, mr);
    Value el = elementwiseDouble(z, rxy,
        [](double zz, double rr) { return std::atan2(zz, rr); }, mr);
    Value r = elementwiseDouble(rxy, z,
        [](double rxy_v, double zz) { return std::hypot(rxy_v, zz); }, mr);
    return { std::move(az), std::move(el), std::move(r) };
}

CartTriple sph2cart(std::pmr::memory_resource *mr,
                    const Value &az, const Value &el, const Value &r)
{
    // r * cos(el)
    Value rcos_el = elementwiseDouble(r, el,
        [](double rr, double ee) { return rr * std::cos(ee); }, mr);
    Value xv = elementwiseDouble(rcos_el, az,
        [](double rce, double aa) { return rce * std::cos(aa); }, mr);
    Value yv = elementwiseDouble(rcos_el, az,
        [](double rce, double aa) { return rce * std::sin(aa); }, mr);
    Value zv = elementwiseDouble(r, el,
        [](double rr, double ee) { return rr * std::sin(ee); }, mr);
    return { std::move(xv), std::move(yv), std::move(zv) };
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

NK_UNARY_ADAPTER(sec,   sec)
NK_UNARY_ADAPTER(csc,   csc)
NK_UNARY_ADAPTER(cot,   cot)
NK_UNARY_ADAPTER(sech,  sech)
NK_UNARY_ADAPTER(csch,  csch)
NK_UNARY_ADAPTER(coth,  coth)
NK_UNARY_ADAPTER(secd,  secd)
NK_UNARY_ADAPTER(cscd,  cscd)
NK_UNARY_ADAPTER(cotd,  cotd)
NK_UNARY_ADAPTER(asec,  asec)
NK_UNARY_ADAPTER(acsc,  acsc)
NK_UNARY_ADAPTER(acot,  acot)
NK_UNARY_ADAPTER(asech, asech)
NK_UNARY_ADAPTER(acsch, acsch)
NK_UNARY_ADAPTER(acoth, acoth)
NK_UNARY_ADAPTER(asecd, asecd)
NK_UNARY_ADAPTER(acscd, acscd)
NK_UNARY_ADAPTER(acotd, acotd)

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

void cart2pol_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("cart2pol: requires at least 2 arguments",
                     0, 0, "cart2pol", "", "m:cart2pol:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() >= 3) {
        auto [theta, rho, z] = cart2pol(mr, args[0], args[1], args[2]);
        outs[0] = std::move(theta);
        if (nargout > 1) outs[1] = std::move(rho);
        if (nargout > 2) outs[2] = std::move(z);
        return;
    }
    auto [theta, rho] = cart2pol(mr, args[0], args[1]);
    outs[0] = std::move(theta);
    if (nargout > 1) outs[1] = std::move(rho);
}

void pol2cart_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pol2cart: requires at least 2 arguments",
                     0, 0, "pol2cart", "", "m:pol2cart:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() >= 3) {
        auto [xv, yv, zv] = pol2cart(mr, args[0], args[1], args[2]);
        outs[0] = std::move(xv);
        if (nargout > 1) outs[1] = std::move(yv);
        if (nargout > 2) outs[2] = std::move(zv);
        return;
    }
    auto [xv, yv] = pol2cart(mr, args[0], args[1]);
    outs[0] = std::move(xv);
    if (nargout > 1) outs[1] = std::move(yv);
}

void cart2sph_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("cart2sph: requires 3 arguments",
                     0, 0, "cart2sph", "", "m:cart2sph:nargin");
    auto [az, el, r] = cart2sph(ctx.engine->resource(), args[0], args[1], args[2]);
    outs[0] = std::move(az);
    if (nargout > 1) outs[1] = std::move(el);
    if (nargout > 2) outs[2] = std::move(r);
}

void sph2cart_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("sph2cart: requires 3 arguments",
                     0, 0, "sph2cart", "", "m:sph2cart:nargin");
    auto [xv, yv, zv] = sph2cart(ctx.engine->resource(), args[0], args[1], args[2]);
    outs[0] = std::move(xv);
    if (nargout > 1) outs[1] = std::move(yv);
    if (nargout > 2) outs[2] = std::move(zv);
}

} // namespace detail

} // namespace numkit::builtin
