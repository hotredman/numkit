// src/builtin/src/elfun/trigonometry.cpp
//
// Trigonometric coordinate transforms, degree conversions, angle wrapping.

#include <numkit/builtin/elfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/ops/helpers.hpp>

#include <cmath>
#include <complex>
#include "_unary_hint.hpp"

namespace numkit::builtin {

Value sin(const Value &x, std::pmr::memory_resource *mr) { return sin(x, nullptr, mr); }
Value cos(const Value &x, std::pmr::memory_resource *mr) { return cos(x, nullptr, mr); }

// ── Degree / Radian conversions ─────────────────────────────────────────────

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

// ── Angle Wrapping ──────────────────────────────────────────────────────────

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline double wrapMod(double x, double period)
{
    return x - std::floor(x / period) * period;
}

inline double wrapToUpper(double x, double period)
{
    const bool positiveInput = x > 0.0;
    double m = wrapMod(x, period);
    if (m == 0.0 && positiveInput) m = period;
    return m;
}

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

// ── Coordinate transforms ───────────────────────────────────────────────────

PolarPair cart2pol(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    Value theta = atan2(y, x, mr);
    Value rho   = hypot(x, y, mr);
    return { std::move(theta), std::move(rho) };
}

CylTriple cart2pol(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr)
{
    auto [theta, rho] = cart2pol(x, y, mr);
    return { std::move(theta), std::move(rho), z };
}

CartPair pol2cart(const Value &theta, const Value &rho, std::pmr::memory_resource *mr)
{
    Value xv = elementwiseDouble(rho, theta,
        [](double r, double t) { return r * std::cos(t); }, mr);
    Value yv = elementwiseDouble(rho, theta,
        [](double r, double t) { return r * std::sin(t); }, mr);
    return { std::move(xv), std::move(yv) };
}

CartTriple pol2cart(const Value &theta, const Value &rho, const Value &z, std::pmr::memory_resource *mr)
{
    auto [xv, yv] = pol2cart(theta, rho, mr);
    return { std::move(xv), std::move(yv), z };
}

SphTriple cart2sph(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr)
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

CartTriple sph2cart(const Value &az, const Value &el, const Value &r, std::pmr::memory_resource *mr)
{
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

Value cart2pol(Span<const Value> args, std::pmr::memory_resource *mr)
{
    if (args.size() < 2)
        throw Error("cart2pol: requires at least 2 arguments (x, y)",
                     0, 0, "cart2pol", "", "numkit:cart2pol:nargin");
    return cart2pol(args[0], args[1], mr).theta;
}

Value pol2cart(Span<const Value> args, std::pmr::memory_resource *mr)
{
    if (args.size() < 2)
        throw Error("pol2cart: requires at least 2 arguments (theta, rho)",
                     0, 0, "pol2cart", "", "numkit:pol2cart:nargin");
    return pol2cart(args[0], args[1], mr).x;
}

Value cart2sph(Span<const Value> args, std::pmr::memory_resource *mr)
{
    if (args.size() < 3)
        throw Error("cart2sph: requires 3 arguments (x, y, z)",
                     0, 0, "cart2sph", "", "numkit:cart2sph:nargin");
    return cart2sph(args[0], args[1], args[2], mr).az;
}

Value sph2cart(Span<const Value> args, std::pmr::memory_resource *mr)
{
    if (args.size() < 3)
        throw Error("sph2cart: requires 3 arguments (az, el, r)",
                     0, 0, "sph2cart", "", "numkit:sph2cart:nargin");
    return sph2cart(args[0], args[1], args[2], mr).x;
}

} // namespace numkit::builtin
