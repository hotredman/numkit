// toolboxes/builtin/src/math/trig/trigonometry.cpp
//
// Trig functions whose implementations don't fit the simple SIMD
// backend pattern: the degree / multiple-of-π variants. The
// SIMD-friendly trig kernels (sin, cos, tan, sinh, cosh, tanh, asin,
// acos, atan, atan2, asinh, acosh, atanh) live in trig_highway.cpp /
// trig_portable.cpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/arithmetic/misc.hpp>          // hypot decl
#include <numkit/builtin/math/trig/trigonometry.hpp>

#include "../_unary_hint.hpp"   // 3-arg sin/cos hint overloads

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::math {

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

// Public 2-arg wrappers — delegate to the 3-arg overload in the SIMD
// backends (trig_highway.cpp / trig_portable.cpp) with no buffer hint.
Value sin(const Value &x, std::pmr::memory_resource *mr) { return sin(x, nullptr, mr); }
Value cos(const Value &x, std::pmr::memory_resource *mr) { return cos(x, nullptr, mr); }

// sind / cosd / tand / asind / acosd / atand / atan2d / sinpi / cospi
// now live in trig_highway.cpp / trig_portable.cpp. The snap-helpers
// (sind_scalar, cosd_scalar, tand_scalar) remain in this file's
// anonymous namespace because secd / cscd / cotd reuse them below.

// Reciprocal trig (sec/csc/cot families and inverses) live in the
// SIMD-backend pair `trig_recip_highway.cpp` / `trig_recip_portable.cpp`.
// Coord transforms remain here.

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

PolarPair cart2pol(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    // Compose via the SIMD-dispatched atan2 + hypot (in trig_highway.cpp).
    // Each call gets the full vector SIMD path on real-same-shape inputs.
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

} // namespace numkit::math
