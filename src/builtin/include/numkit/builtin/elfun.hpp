// include/numkit/builtin/elfun.hpp
//
// Elementary mathematical functions: trigonometry, exp, log, complex, rounding.
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @file
/// @ingroup group_elfun
/// @brief Elementary mathematical functions (trigonometry, exp, log, complex numbers, rounding).
///
/// Provides an engine-free, highly optimized C++ interface for MATLAB-compatible elementary mathematical functions,
/// featuring Highway SIMD vector acceleration and thread parallelism.

// ── Trigonometry (Radians) ──────────────────────────────────────────────────

/// @brief Elementwise sine in radians (`y = sin(x)`).
///
/// Computes trigonometric sine for real and complex input arrays.
/// Accelerated with Highway SIMD vectorization.
///
/// @param x Input array in radians.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `sin(x)`.
/// @see cos, tan, asin, sind, sinpi
Value sin(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise cosine in radians (`y = cos(x)`).
///
/// Computes trigonometric cosine for real and complex input arrays.
/// Accelerated with Highway SIMD vectorization.
///
/// @param x Input array in radians.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `cos(x)`.
/// @see sin, tan, acos, cosd, cospi
Value cos(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise tangent in radians (`y = tan(x)`).
///
/// Computes trigonometric tangent (`sin(x) / cos(x)`).
///
/// @param x Input array in radians.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `tan(x)`.
/// @see sin, cos, atan, tand
Value tan(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse sine in radians (`y = asin(x)`).
///
/// Computes arcsine. Returns real values in `[-pi/2, pi/2]` for domain `[-1, 1]`;
/// generates complex outputs for values outside the real domain.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `asin(x)`.
/// @see sin, acos, atan, asind
Value asin(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse cosine in radians (`y = acos(x)`).
///
/// Computes arccosine. Returns real values in `[0, pi]` for domain `[-1, 1]`;
/// generates complex outputs for values outside the real domain.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `acos(x)`.
/// @see cos, asin, atan, acosd
Value acos(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse tangent in radians (`y = atan(x)`).
///
/// Computes arctangent. Returns real values in `(-pi/2, pi/2)`.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `atan(x)`.
/// @see atan2, tan, asin, acos, atand
Value atan(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Four-quadrant inverse tangent in radians (`theta = atan2(y, x)`).
///
/// Computes the angle in the Euclidean plane between the positive x-axis and the point (x, y).
/// Handles broadcast shapes between `y` and `x`.
///
/// @param y Y coordinates.
/// @param x X coordinates.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Broadcast array containing `atan2(y, x)` in `[-pi, pi]`.
/// @see atan, atan2d, cart2pol
Value atan2(const Value &y, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise secant in radians (`y = sec(x) = 1 / cos(x)`).
///
/// @param x Input array in radians.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `sec(x)`.
/// @see cos, csc, cot, secd
Value sec(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise cosecant in radians (`y = csc(x) = 1 / sin(x)`).
///
/// @param x Input array in radians.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `csc(x)`.
/// @see sin, sec, cot, cscd
Value csc(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise cotangent in radians (`y = cot(x) = 1 / tan(x)`).
///
/// @param x Input array in radians.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `cot(x)`.
/// @see tan, sec, csc, cotd
Value cot(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse secant in radians (`y = asec(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `asec(x)`.
/// @see sec, acsc, acot, asecd
Value asec(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse cosecant in radians (`y = acsc(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `acsc(x)`.
/// @see csc, asec, acot, acscd
Value acsc(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse cotangent in radians (`y = acot(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `acot(x)`.
/// @see cot, asec, acsc, acotd
Value acot(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Trigonometry (Degrees) ──────────────────────────────────────────────────

/// @brief Elementwise sine in degrees (`y = sind(x)`).
///
/// Computes trigonometric sine for angles specified in degrees.
///
/// @param x Input array in degrees.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `sind(x)`.
/// @see sin, cosd, tand, asind
Value sind(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise cosine in degrees (`y = cosd(x)`).
///
/// Computes trigonometric cosine for angles specified in degrees.
///
/// @param x Input array in degrees.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `cosd(x)`.
/// @see cos, sind, tand, acosd
Value cosd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise tangent in degrees (`y = tand(x)`).
///
/// Computes trigonometric tangent for angles specified in degrees.
///
/// @param x Input array in degrees.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `tand(x)`.
/// @see tan, sind, cosd, atand
Value tand(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse sine in degrees (`y = asind(x)`).
///
/// Returns values in `[-90, 90]` degrees.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `asind(x)`.
/// @see asin, sind, acosd, atand
Value asind(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse cosine in degrees (`y = acosd(x)`).
///
/// Returns values in `[0, 180]` degrees.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `acosd(x)`.
/// @see acos, cosd, asind, atand
Value acosd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse tangent in degrees (`y = atand(x)`).
///
/// Returns values in `(-90, 90)` degrees.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `atand(x)`.
/// @see atan, tand, atan2d, asind
Value atand(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Four-quadrant inverse tangent in degrees (`theta = atan2d(y, x)`).
///
/// Computes angle in degrees in `[-180, 180]`. Handles broadcast shapes.
///
/// @param y Y coordinates.
/// @param x X coordinates.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Broadcast array containing `atan2d(y, x)` in `[-180, 180]` degrees.
/// @see atan2, atand
Value atan2d(const Value &y, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise secant in degrees (`y = secd(x)`).
///
/// @param x Input array in degrees.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `secd(x)`.
/// @see sec, cosd, cscd, cotd
Value secd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise cosecant in degrees (`y = cscd(x)`).
///
/// @param x Input array in degrees.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `cscd(x)`.
/// @see csc, sind, secd, cotd
Value cscd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise cotangent in degrees (`y = cotd(x)`).
///
/// @param x Input array in degrees.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `cotd(x)`.
/// @see cot, tand, secd, cscd
Value cotd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse secant in degrees (`y = asecd(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `asecd(x)` in degrees.
/// @see asec, secd, acscd, acotd
Value asecd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse cosecant in degrees (`y = acscd(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `acscd(x)` in degrees.
/// @see acsc, cscd, asecd, acotd
Value acscd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse cotangent in degrees (`y = acotd(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `acotd(x)` in degrees.
/// @see acot, cotd, asecd, acscd
Value acotd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise sine of pi times input (`y = sin(pi * x)`).
///
/// Computes `sin(pi * x)` with exact zeros at all integer inputs.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `sin(pi * x)`.
/// @see sin, cospi
Value sinpi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise cosine of pi times input (`y = cos(pi * x)`).
///
/// Computes `cos(pi * x)` with exact zeros at odd half-integers.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `cos(pi * x)`.
/// @see cos, sinpi
Value cospi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts angles from degrees to radians (`y = deg2rad(x)`).
///
/// Computes `x * (pi / 180)`.
///
/// @param x Angle array in degrees.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Angle array in radians.
/// @see rad2deg, sin, cos
Value deg2rad(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts angles from radians to degrees (`y = rad2deg(x)`).
///
/// Computes `x * (180 / pi)`.
///
/// @param x Angle array in radians.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Angle array in degrees.
/// @see deg2rad, sind, cosd
Value rad2deg(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Wraps angles in radians to interval `[-pi, pi]`.
///
/// @param x Angle array in radians.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Wrapped angle array in `[-pi, pi]`.
/// @see wrapTo2Pi, wrapTo180, wrapTo360
Value wrapToPi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Wraps angles in radians to interval `[0, 2*pi)`.
///
/// @param x Angle array in radians.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Wrapped angle array in `[0, 2*pi)`.
/// @see wrapToPi, wrapTo360
Value wrapTo2Pi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Wraps angles in degrees to interval `[-180, 180]`.
///
/// @param x Angle array in degrees.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Wrapped angle array in `[-180, 180]`.
/// @see wrapTo360, wrapToPi
Value wrapTo180(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Wraps angles in degrees to interval `[0, 360)`.
///
/// @param x Angle array in degrees.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Wrapped angle array in `[0, 360)`.
/// @see wrapTo180, wrapTo2Pi
Value wrapTo360(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Hyperbolic Functions ────────────────────────────────────────────────────

/// @brief Hyperbolic sine (`y = sinh(x)`).
///
/// Computes `(exp(x) - exp(-x)) / 2` elementwise for real and complex arrays.
/// Accelerated with Highway SIMD vectorization.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `sinh(x)`.
/// @see cosh, tanh, asinh, csch
Value sinh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic cosine (`y = cosh(x)`).
///
/// Computes `(exp(x) + exp(-x)) / 2` elementwise for real and complex arrays.
/// Accelerated with Highway SIMD vectorization.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `cosh(x)`.
/// @see sinh, tanh, acosh, sech
Value cosh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic tangent (`y = tanh(x)`).
///
/// Computes `sinh(x) / cosh(x)` elementwise for real and complex arrays.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `tanh(x)`.
/// @see sinh, cosh, atanh, coth
Value tanh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic sine (`y = asinh(x)`).
///
/// Computes `log(x + sqrt(x^2 + 1))` elementwise.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `asinh(x)`.
/// @see sinh, acosh, atanh
Value asinh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic cosine (`y = acosh(x)`).
///
/// Computes `log(x + sqrt(x^2 - 1))` elementwise for real values `x >= 1`
/// (and complex outputs for `x < 1`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `acosh(x)`.
/// @see cosh, asinh, atanh
Value acosh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic tangent (`y = atanh(x)`).
///
/// Computes `0.5 * log((1 + x) / (1 - x))` elementwise for real values `|x| < 1`.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `atanh(x)`.
/// @see tanh, asinh, acosh
Value atanh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic secant (`y = sech(x) = 1 / cosh(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `sech(x)`.
/// @see cosh, csch, asech
Value sech(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic cosecant (`y = csch(x) = 1 / sinh(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `csch(x)`.
/// @see sinh, sech, acsch
Value csch(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic cotangent (`y = coth(x) = 1 / tanh(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `coth(x)`.
/// @see tanh, csch, acoth
Value coth(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic secant (`y = asech(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `asech(x)`.
/// @see sech, acsch, acoth
Value asech(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic cosecant (`y = acsch(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `acsch(x)`.
/// @see csch, asech, acoth
Value acsch(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic cotangent (`y = acoth(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `acoth(x)`.
/// @see coth, asech, acsch
Value acoth(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Coordinate Transforms ───────────────────────────────────────────────────

/// @brief Result of 2D Cartesian to polar conversion (`[theta, rho] = cart2pol(x, y)`).
struct PolarPair {
    Value theta; ///< Angle in radians in interval `[-pi, pi]`.
    Value rho;   ///< Radial distance from origin (`sqrt(x^2 + y^2)`).
};

/// @brief Result of 3D Cartesian to cylindrical conversion (`[theta, rho, z] = cart2pol(x, y, z)`).
struct CylTriple {
    Value theta; ///< Azimuth angle in radians in interval `[-pi, pi]`.
    Value rho;   ///< Radial distance in xy-plane (`sqrt(x^2 + y^2)`).
    Value z;     ///< Height along z-axis.
};

/// @brief Result of 2D polar to Cartesian conversion (`[x, y] = pol2cart(theta, rho)`).
struct CartPair {
    Value x; ///< X coordinate (`rho .* cos(theta)`).
    Value y; ///< Y coordinate (`rho .* sin(theta)`).
};

/// @brief Result of 3D cylindrical/spherical to Cartesian conversion (`[x, y, z]`).
struct CartTriple {
    Value x; ///< X coordinate.
    Value y; ///< Y coordinate.
    Value z; ///< Z coordinate.
};

/// @brief Result of 3D Cartesian to spherical conversion (`[az, el, r] = cart2sph(x, y, z)`).
struct SphTriple {
    Value az; ///< Azimuth angle in radians in interval `[-pi, pi]`.
    Value el; ///< Elevation angle in radians in interval `[-pi/2, pi/2]`.
    Value r;  ///< Radial distance from origin (`sqrt(x^2 + y^2 + z^2)`).
};

/// @brief Transforms 2D Cartesian coordinates to polar coordinates (`[theta, rho] = cart2pol(x, y)`).
///
/// Computes `theta = atan2(y, x)` and `rho = hypot(x, y)` elementwise.
///
/// @param x X Cartesian coordinates.
/// @param y Y Cartesian coordinates.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return PolarPair containing angle `theta` (radians) and radius `rho`.
/// @see pol2cart, cart2sph, sph2cart
PolarPair cart2pol(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 3D Cartesian coordinates to cylindrical coordinates (`[theta, rho, z] = cart2pol(x, y, z)`).
///
/// @param x X Cartesian coordinates.
/// @param y Y Cartesian coordinates.
/// @param z Z Cartesian coordinates.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return CylTriple containing `theta`, `rho`, and `z`.
/// @see pol2cart, cart2sph, sph2cart
CylTriple cart2pol(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 2D polar coordinates to Cartesian coordinates (`[x, y] = pol2cart(theta, rho)`).
///
/// Computes `x = rho .* cos(theta)` and `y = rho .* sin(theta)` elementwise.
///
/// @param theta Angle array in radians.
/// @param rho Radial distance array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return CartPair containing `x` and `y` coordinates.
/// @see cart2pol, sph2cart, cart2sph
CartPair pol2cart(const Value &theta, const Value &rho, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 3D cylindrical coordinates to Cartesian coordinates (`[x, y, z] = pol2cart(theta, rho, z)`).
///
/// @param theta Azimuth angle array in radians.
/// @param rho Radial distance in xy-plane array.
/// @param z Height coordinate array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return CartTriple containing `x`, `y`, and `z`.
/// @see cart2pol, sph2cart, cart2sph
CartTriple pol2cart(const Value &theta, const Value &rho, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 3D Cartesian coordinates to spherical coordinates (`[az, el, r] = cart2sph(x, y, z)`).
///
/// Computes `az = atan2(y, x)`, `el = atan2(z, hypot(x, y))`, and `r = sqrt(x^2 + y^2 + z^2)`.
///
/// @param x X Cartesian coordinates.
/// @param y Y Cartesian coordinates.
/// @param z Z Cartesian coordinates.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return SphTriple containing azimuth `az`, elevation `el`, and radius `r`.
/// @see sph2cart, cart2pol, pol2cart
SphTriple cart2sph(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 3D spherical coordinates to Cartesian coordinates (`[x, y, z] = sph2cart(az, el, r)`).
///
/// Computes `x = r .* cos(el) .* cos(az)`, `y = r .* cos(el) .* sin(az)`, and `z = r .* sin(el)`.
///
/// @param az Azimuth angle in radians.
/// @param el Elevation angle in radians.
/// @param r Radial distance from origin.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return CartTriple containing `x`, `y`, and `z`.
/// @see cart2sph, pol2cart, cart2pol
CartTriple sph2cart(const Value &az, const Value &el, const Value &r, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 2D Cartesian coordinates to polar coordinates (span wrapper).
/// @param args Span containing `x` and `y`.
/// @param mr Memory resource.
/// @return Struct containing `{th, r}`.
Value cart2pol(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 2D polar coordinates to Cartesian coordinates (span wrapper).
/// @param args Span containing `th` and `r`.
/// @param mr Memory resource.
/// @return Struct containing `{x, y}`.
Value pol2cart(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 3D Cartesian coordinates to spherical coordinates (span wrapper).
/// @param args Span containing `x`, `y`, `z`.
/// @param mr Memory resource.
/// @return Struct containing `{az, el, r}`.
Value cart2sph(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 3D spherical coordinates to Cartesian coordinates (span wrapper).
/// @param args Span containing `az`, `el`, `r`.
/// @param mr Memory resource.
/// @return Struct containing `{x, y, z}`.
Value sph2cart(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

// ── Exponentials and Logarithms ─────────────────────────────────────────────

/// @brief Natural exponential (`y = exp(x)`).
///
/// Computes `e^x` elementwise for real and complex input arrays.
/// Accelerated with Highway SIMD vectorization.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `exp(x)`.
/// @see expm1, log, log10, log2, pow2
Value exp(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes `exp(x) - 1` accurately for small `x`.
///
/// Preserves numerical precision for values of `x` very close to 0.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `exp(x) - 1`.
/// @see exp, log1p
Value expm1(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Natural logarithm (`ln(x)`).
///
/// Computes natural logarithm elementwise. Produces complex results for negative real inputs.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `log(x)`.
/// @see exp, log10, log2, log1p, reallog
Value log(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Base-10 logarithm (`log10(x)`).
///
/// Computes common logarithm elementwise.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `log10(x)`.
/// @see log, log2
Value log10(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Base-2 logarithm (`log2(x)`).
///
/// Computes binary logarithm elementwise.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `log2(x)`.
/// @see log, log10, pow2, nextpow2
Value log2(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes `log(1 + x)` accurately for small `x`.
///
/// Preserves precision when `|x| << 1`.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `log(1 + x)`.
/// @see log, expm1
Value log1p(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Base-2 power scaling (`y = 2.^x`).
///
/// @param x Power exponent array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `2.^x`.
/// @see log2, pow, nextpow2
Value pow2(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Base-2 power scaling with mantissa and exponent (`y = f .* 2.^e`).
///
/// Equivalent to standard `ldexp(f, e)` elementwise.
///
/// @param f Mantissa array.
/// @param e Exponent array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Scaled values `f .* 2.^e`.
/// @see log2, pow2
Value pow2(const Value &f, const Value &e, std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest integer exponent such that `2^p >= abs(n)` (`p = nextpow2(n)`).
///
/// Useful for determining optimal FFT buffer lengths.
///
/// @param n Input array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Exponent array `p = ceil(log2(abs(n)))`.
/// @see pow2, log2
Value nextpow2(const Value &n, std::pmr::memory_resource *mr = nullptr);

/// @brief Square root (`y = sqrt(x)`).
///
/// Computes square root elementwise. Produces complex numbers for negative inputs.
/// Accelerated with Highway SIMD vectorization.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `sqrt(x)`.
/// @see realsqrt, cbrt, nthroot, hypot
Value sqrt(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Real square root with domain error check (`y = realsqrt(x)`).
///
/// Computes square root elementwise, raising a runtime domain error if any element is negative.
///
/// @param x Input array of non-negative real numbers.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Real array containing `sqrt(x)`.
/// @see sqrt, reallog, realpow
Value realsqrt(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Real natural logarithm with domain error check (`y = reallog(x)`).
///
/// Computes natural log elementwise, raising a runtime error if any element is non-positive.
///
/// @param x Input array of positive real numbers.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Real array containing `log(x)`.
/// @see log, realsqrt, realpow
Value reallog(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cube root (`y = cbrt(x)`).
///
/// Computes real cube root elementwise.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `cbrt(x)`.
/// @see sqrt, nthroot
Value cbrt(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Real nth root of real numbers (`y = nthroot(x, n)`).
///
/// Computes the real nth root of elements in `x`. Handles negative `x` for odd integer `n`.
///
/// @param x Real base array.
/// @param n Real root degree.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `nthroot(x, n)`.
/// @see sqrt, cbrt, pow
Value nthroot(const Value &x, const Value &n, std::pmr::memory_resource *mr = nullptr);

/// @brief Square root of sum of squares (`hypot(x, y) = sqrt(x^2 + y^2)`).
///
/// Computes the hypotenuse accurately without intermediate overflow or underflow.
///
/// @param x First argument array.
/// @param y Second argument array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Broadcast array containing `sqrt(x^2 + y^2)`.
/// @see sqrt, abs, cart2pol
Value hypot(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise power (`z = x .^ y`).
///
/// Computes `x` raised to the power `y` elementwise. Supports scalar-array and array-array broadcasting.
///
/// @param x Base array.
/// @param y Exponent array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Broadcast array containing `x .^ y`.
/// @see exp, pow2, realpow
Value pow(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Real power with complex error check (`y = realpow(x, y)`).
///
/// Computes `x .^ y`, raising a runtime error if the result would be complex.
///
/// @param x Base array.
/// @param y Exponent array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array containing `x .^ y`.
/// @see pow, realsqrt, reallog
Value realpow(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

// ── Complex Numbers ─────────────────────────────────────────────────────────

/// @brief Real part of complex array (`y = real(x)`).
///
/// Extracts the real component of each element.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Real array containing `real(x)`.
/// @see imag, conj, complex, isreal
Value real(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Imaginary part of complex array (`y = imag(x)`).
///
/// Extracts the imaginary component of each element.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Real array containing `imag(x)` (zeros for real inputs).
/// @see real, conj, complex, angle
Value imag(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Complex conjugate (`y = conj(x)`).
///
/// Inverts the sign of the imaginary component (`a + bi -> a - bi`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `conj(x)`.
/// @see real, imag, abs, angle
Value conj(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Phase angle of complex numbers in radians (`y = angle(x)`).
///
/// Computes `atan2(imag(x), real(x))` in the range `[-pi, pi]`.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing phase angles in radians.
/// @see abs, unwrap, atan2
Value angle(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Absolute value / complex magnitude (`y = abs(x)`).
///
/// Computes `|x|` for real numbers and `sqrt(re^2 + im^2)` for complex numbers.
/// Accelerated with Highway SIMD vectorization.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing magnitude `|x|`.
/// @see angle, hypot, sign
Value abs(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Constructs complex array from real part (`complex(re)`).
///
/// @param re Real components (imaginary set to zero).
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Complex array `re + 0i`.
/// @see real, imag, isreal
Value complex(const Value &re, std::pmr::memory_resource *mr = nullptr);

/// @brief Constructs complex array from real and imaginary parts (`complex(re, im)`).
///
/// @param re Real components.
/// @param im Imaginary components.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Complex array `re + im * 1i`.
/// @see real, imag, conj
Value complex(const Value &re, const Value &im, std::pmr::memory_resource *mr = nullptr);

/// @brief Signum function (-1, 0, 1 or unit complex phasor).
///
/// For real numbers, returns `1` for `x > 0`, `-1` for `x < 0`, and `0` for `x == 0`.
/// For complex numbers, returns `x ./ abs(x)`.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `sign(x)`.
/// @see abs
Value sign(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Unwraps phase angles across array dimensions.
///
/// Corrects the radian phase angles in `p` by adding multiples of `±2*pi`
/// whenever jump discontinuities exceed `tol`.
///
/// @param p Phase angle array in radians.
/// @param tol Jump tolerance threshold (default: pi).
/// @param dim Dimension along which to operate (-1 for first non-singleton dimension).
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array with unwrapped continuous phase.
/// @see angle, wrapToPi
Value unwrap(const Value &p, double tol = 3.14159265358979323846, int dim = -1, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if array contains real values (false if complex storage is active).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Scalar logical Value (`true` if real, `false` if complex).
/// @see real, imag, isnumeric
Value isreal(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Rounding and Remainder ──────────────────────────────────────────────────

/// @brief Rounds elements to the nearest integer (`y = round(x)`).
///
/// Rounds half-integers away from zero (MATLAB round semantics).
/// Accelerated with Highway SIMD vectorization.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape with rounded elements.
/// @see floor, ceil, fix, roundN
Value round(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Rounds elements to N decimal digits or N significant digits (`round(x, n, type)`).
///
/// @param x Input array.
/// @param n Number of decimal or significant digits.
/// @param significant True to round to significant digits, false for decimal places.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape with rounded elements.
/// @see round, floor, ceil
Value roundN(const Value &x, int n, bool significant = false, std::pmr::memory_resource *mr = nullptr);

/// @brief Rounds down towards negative infinity (`y = floor(x)`).
///
/// Accelerated with Highway SIMD vectorization.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `floor(x)`.
/// @see ceil, round, fix
Value floor(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Rounds up towards positive infinity (`y = ceil(x)`).
///
/// Accelerated with Highway SIMD vectorization.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `ceil(x)`.
/// @see floor, round, fix
Value ceil(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Rounds towards zero / truncate (`y = fix(x)`).
///
/// Truncates fractional part towards zero.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `fix(x)`.
/// @see floor, ceil, round
Value fix(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Positive part / rectifier (`y = max(x, 0)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape with negative elements clamped to zero.
/// @see max, fix
Value subplus(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Modulo operation where sign of result matches divisor (`y = mod(x, y)`).
///
/// Computes `x - y .* floor(x ./ y)` elementwise.
///
/// @param x Dividend array.
/// @param y Divisor array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `mod(x, y)`.
/// @see rem, floor
Value mod(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Remainder after division where sign of result matches dividend (`y = rem(x, y)`).
///
/// Computes `x - y .* fix(x ./ y)` elementwise.
///
/// @param x Dividend array.
/// @param y Divisor array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `rem(x, y)`.
/// @see mod, fix
Value rem(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
