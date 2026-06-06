// libs/builtin/include/numkit/builtin/math/trig/trigonometry.hpp
//
// Trigonometric and hyperbolic builtins.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::builtin {

/// @file
/// @brief Trigonometric / hyperbolic builtins.
///
/// **Conventions across this header:**
/// - All functions are **elementwise** on real arrays of any shape.
/// - Output has the same shape and type as the input (numeric integer
///   types promote to DOUBLE, char to DOUBLE).
/// - `sin` and `cos` go through SIMD-backed transcendentals
///   (`libs/builtin/src/backends/`); the rest are scalar
///   `<cmath>` wrappers.
/// - Degree-suffix forms (`sind`, `cosd`, …) compute in degrees:
///   `sind(x) ≡ sin(x · π / 180)` and likewise for inverses.
/// - Pi-scaled forms (`sinpi`, `cospi`) compute `sin(π·x)` etc.
///   accurately near integer `x`.
///
/// Per-function comments below list only the formula / domain notes;
/// inputs and outputs follow the universal pattern (`x` is the input
/// array, `mr` is the memory resource — nullptr → process default,
/// return value matches `x`'s shape).

// ── Primary trig (radians) ───────────────────────────────────────────

/// @brief Sine (`y = sin(x)`). SIMD-backed.
/// @param x   Input array (radians).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `sin(x)`, same shape as `x`. @see cos, sind, sinpi
Value sin(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cosine (`y = cos(x)`). SIMD-backed.
/// @param x   Input array (radians). @param mr  Memory resource.
/// @return    `cos(x)`. @see sin, cosd, cospi
Value cos(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Tangent (`y = tan(x)`). Scalar; returns `±Inf` at `x = π/2 + kπ`.
/// @param x   Input array (radians). @param mr  Memory resource.
/// @return    `tan(x)`. @see sin, cos, atan
Value tan(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse sine (`y = asin(x)`). Domain `[-1, 1]`; outside → `NaN`.
/// @param x   Input array. @param mr  Memory resource.
/// @return    `asin(x)` in `[-π/2, π/2]`. @see sin, asind
Value asin(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse cosine (`y = acos(x)`). Domain `[-1, 1]`; outside → `NaN`.
/// @param x   Input array. @param mr  Memory resource.
/// @return    `acos(x)` in `[0, π]`. @see cos, acosd
Value acos(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse tangent (`y = atan(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `atan(x)` in `(-π/2, π/2)`. @see tan, atan2, atand
Value atan(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Two-argument inverse tangent (`y = atan2(y, x)`).
///
/// Returns the angle of the point `(x, y)` in `(-π, π]`, with quadrant
/// resolved.
///
/// @param y   y-coordinates.
/// @param x   x-coordinates (broadcasts with `y`).
/// @param mr  Memory resource.
/// @return    `atan2(y, x)` in `(-π, π]`, broadcast shape. @see atan, atan2d
Value atan2(const Value &y, const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Hyperbolic ───────────────────────────────────────────────────────

/// @brief Hyperbolic sine (`y = sinh(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `sinh(x)`. @see cosh, tanh, asinh
Value sinh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic cosine (`y = cosh(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `cosh(x) >= 1`. @see sinh, acosh
Value cosh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic tangent (`y = tanh(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `tanh(x)` in `(-1, 1)`. @see sinh, cosh, atanh
Value tanh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic sine (`y = asinh(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `asinh(x)`. @see sinh
Value asinh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic cosine (`y = acosh(x)`). Domain `x >= 1`.
/// @param x   Input array. @param mr  Memory resource.
/// @return    `acosh(x) >= 0` (NaN for `x < 1`). @see cosh
Value acosh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic tangent (`y = atanh(x)`). Domain `(-1, 1)`.
/// @param x   Input array. @param mr  Memory resource.
/// @return    `atanh(x)` (NaN outside domain). @see tanh
Value atanh(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Degree-input / -output forms ─────────────────────────────────────

/// @brief Sine in degrees (`y = sind(x) = sin(x · π / 180)`).
/// @param x   Input array (degrees). @param mr  Memory resource.
/// @return    `sind(x)`. @see sin
Value sind(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cosine in degrees (`y = cosd(x)`).
/// @param x   Input array (degrees). @param mr  Memory resource.
/// @return    `cosd(x)`. @see cos
Value cosd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Tangent in degrees (`y = tand(x)`).
/// @param x   Input array (degrees). @param mr  Memory resource.
/// @return    `tand(x)`. @see tan
Value tand(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse sine in degrees (`y = asind(x) ∈ [-90, 90]`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `asind(x)`. @see asin
Value asind(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse cosine in degrees (`y = acosd(x) ∈ [0, 180]`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `acosd(x)`. @see acos
Value acosd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse tangent in degrees (`y = atand(x) ∈ (-90, 90)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `atand(x)`. @see atan
Value atand(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Four-quadrant `atan2` in degrees (`y = atan2d(y, x) ∈ (-180, 180]`).
/// @param y   y-coordinates. @param x   x-coordinates (broadcast).
/// @param mr  Memory resource.
/// @return    `atan2d(y, x)`. @see atan2
Value atan2d(const Value &y, const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── π-scaled forms ───────────────────────────────────────────────────

/// @brief π-scaled sine (`y = sinpi(x) = sin(π · x)`).
/// Accurate near integer `x` (where `sin(π·x)` should be exactly 0).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `sinpi(x)`. @see sin, cospi
Value sinpi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief π-scaled cosine (`y = cospi(x) = cos(π · x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `cospi(x)`. @see cos, sinpi
Value cospi(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Reciprocal-trig family ───────────────────────────────────────────

/// @brief Secant (`y = sec(x) = 1 / cos(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `sec(x)`. @see cos, csc, cot
Value sec(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cosecant (`y = csc(x) = 1 / sin(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `csc(x)`. @see sin, sec, cot
Value csc(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cotangent (`y = cot(x) = 1 / tan(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `cot(x)`. @see tan, sec, csc
Value cot(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic secant (`y = sech(x) = 1 / cosh(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `sech(x)`. @see cosh
Value sech(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic cosecant (`y = csch(x) = 1 / sinh(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `csch(x)`. @see sinh
Value csch(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic cotangent (`y = coth(x) = 1 / tanh(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `coth(x)`. @see tanh
Value coth(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Secant in degrees (`y = secd(x) = 1 / cosd(x)`).
/// @param x   Input array (degrees). @param mr  Memory resource.
/// @return    `secd(x)`. @see sec, cosd
Value secd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cosecant in degrees (`y = cscd(x) = 1 / sind(x)`).
/// @param x   Input array (degrees). @param mr  Memory resource.
/// @return    `cscd(x)`. @see csc, sind
Value cscd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cotangent in degrees (`y = cotd(x) = 1 / tand(x)`).
/// @param x   Input array (degrees). @param mr  Memory resource.
/// @return    `cotd(x)`. @see cot, tand
Value cotd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse secant (`y = asec(x) = acos(1/x)`).
/// @param x   Input array (`|x| >= 1`). @param mr  Memory resource.
/// @return    `asec(x)`. @see acos, sec
Value asec(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse cosecant (`y = acsc(x) = asin(1/x)`).
/// @param x   Input array (`|x| >= 1`). @param mr  Memory resource.
/// @return    `acsc(x)`. @see asin, csc
Value acsc(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse cotangent (`y = acot(x) = atan(1/x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `acot(x)`. @see atan, cot
Value acot(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic secant (`y = asech(x) = acosh(1/x)`).
/// @param x   Input array (`x ∈ (0, 1]`). @param mr  Memory resource.
/// @return    `asech(x)`. @see acosh, sech
Value asech(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic cosecant (`y = acsch(x) = asinh(1/x)`).
/// @param x   Input array (`x != 0`). @param mr  Memory resource.
/// @return    `acsch(x)`. @see asinh, csch
Value acsch(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic cotangent (`y = acoth(x) = atanh(1/x)`).
/// @param x   Input array (`|x| > 1`). @param mr  Memory resource.
/// @return    `acoth(x)`. @see atanh, coth
Value acoth(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse secant in degrees (`y = asecd(x) = acosd(1/x)`).
/// @param x   Input array (`|x| >= 1`). @param mr  Memory resource.
/// @return    `asecd(x)`. @see asec
Value asecd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse cosecant in degrees (`y = acscd(x) = asind(1/x)`).
/// @param x   Input array (`|x| >= 1`). @param mr  Memory resource.
/// @return    `acscd(x)`. @see acsc
Value acscd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse cotangent in degrees (`y = acotd(x) = atand(1/x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    `acotd(x)`. @see acot
Value acotd(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Coordinate transforms ────────────────────────────────────────────

/// @brief 2-D polar pair (struct returned by @ref cart2pol 2-arg form).
struct PolarPair {
    Value theta;  ///< Angle in radians.
    Value rho;    ///< Radial distance.
};

/// @brief Cylindrical triple (struct returned by @ref cart2pol 3-arg form).
struct CylTriple {
    Value theta;  ///< Angle in radians.
    Value rho;    ///< Radial distance.
    Value z;      ///< Axial coordinate (passed through).
};

/// @brief Cartesian 2-D pair (struct returned by @ref pol2cart 2-arg form).
struct CartPair {
    Value x;  ///< x-coordinates.
    Value y;  ///< y-coordinates.
};

/// @brief Cartesian 3-D triple (struct returned by @ref pol2cart /
///        @ref sph2cart 3-arg forms).
struct CartTriple {
    Value x;  ///< x-coordinates.
    Value y;  ///< y-coordinates.
    Value z;  ///< z-coordinates.
};

/// @brief Spherical triple (struct returned by @ref cart2sph).
struct SphTriple {
    Value az;  ///< Azimuth angle in radians.
    Value el;  ///< Elevation angle in radians.
    Value r;   ///< Radial distance.
};

/// @brief Cartesian → polar (`[theta, rho] = cart2pol(x, y)`).
///
/// `theta = atan2(y, x)`, `rho = hypot(x, y)`.
///
/// @param x   x-coordinates.
/// @param y   y-coordinates.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `{theta, rho}` pair, broadcast shape.
/// @see pol2cart
PolarPair cart2pol(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Cartesian → cylindrical (`[theta, rho, z] = cart2pol(x, y, z)`).
///
/// 3-arg form passes `z` through unchanged.
///
/// @param x   x-coordinates.
/// @param y   y-coordinates.
/// @param z   z-coordinates.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `{theta, rho, z}` triple, broadcast shape.
/// @see pol2cart
CylTriple cart2pol(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Polar → Cartesian (`[x, y] = pol2cart(theta, rho)`).
///
/// `x = rho · cos(theta)`, `y = rho · sin(theta)`.
///
/// @param theta  Angle in radians.
/// @param rho    Radial distance.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `{x, y}` pair, broadcast shape.
/// @see cart2pol
CartPair pol2cart(const Value &theta, const Value &rho, std::pmr::memory_resource *mr = nullptr);

/// @brief Cylindrical → Cartesian (`[x, y, z] = pol2cart(theta, rho, z)`).
///
/// @param theta  Angle in radians.
/// @param rho    Radial distance.
/// @param z      Axial coordinate.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `{x, y, z}` triple, broadcast shape.
/// @see cart2pol
CartTriple pol2cart(const Value &theta, const Value &rho, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Cartesian → spherical (`[az, el, r] = cart2sph(x, y, z)`).
///
/// `az = atan2(y, x)`, `el = atan2(z, hypot(x, y))`,
/// `r = norm([x, y, z])`. Elevation, not inclination.
///
/// @param x   x-coordinates.
/// @param y   y-coordinates.
/// @param z   z-coordinates.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `{az, el, r}` triple, broadcast shape.
/// @see sph2cart
SphTriple cart2sph(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Spherical → Cartesian (`[x, y, z] = sph2cart(az, el, r)`).
///
/// `x = r·cos(el)·cos(az)`, `y = r·cos(el)·sin(az)`, `z = r·sin(el)`.
///
/// @param az  Azimuth angle in radians.
/// @param el  Elevation angle in radians.
/// @param r   Radial distance.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `{x, y, z}` triple, broadcast shape.
/// @see cart2sph
CartTriple sph2cart(const Value &az, const Value &el, const Value &r, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
