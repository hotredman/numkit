// include/numkit/builtin/elfun.hpp
//
// Elementary mathematical functions: trigonometry, exp, log, complex, rounding.
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @file
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
/// Computes arctangent in `[-pi/2, pi/2]`.
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `atan(x)`.
/// @see tan, atan2, atand
Value atan(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Four-quadrant inverse tangent in radians (`z = atan2(y, x)`).
///
/// Computes arctangent of `y / x` using signs of both arguments to determine quadrant in `[-pi, pi]`.
/// Supports singleton expansion (broadcasting).
///
/// @param y Y coordinates.
/// @param x X coordinates.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Broadcast array containing `atan2(y, x)`.
/// @see atan, atan2d, hypot
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
/// @see sec, acsc, acot
Value asec(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse cosecant in radians (`y = acsc(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `acsc(x)`.
/// @see csc, asec, acot
Value acsc(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse cotangent in radians (`y = acot(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `acot(x)`.
/// @see cot, asec, acsc
Value acot(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Trigonometry (Degrees & Multiples of Pi) ────────────────────────────────

/// @brief Elementwise sine for angles in degrees (`y = sind(x)`).
///
/// Snaps exact integer multiples of 180° to 0, and multiples of 90° to ±1.
///
/// @param x Input array in degrees.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `sind(x)`.
/// @see sin, cosd, tand, asind
Value sind(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise cosine for angles in degrees (`y = cosd(x)`).
///
/// Snaps exact odd multiples of 90° to 0, and multiples of 180° to ±1.
///
/// @param x Input array in degrees.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `cosd(x)`.
/// @see cos, sind, tand, acosd
Value cosd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise tangent for angles in degrees (`y = tand(x)`).
///
/// @param x Input array in degrees.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `tand(x)`.
/// @see tan, sind, cosd, atand
Value tand(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse sine in degrees (`y = asind(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `asind(x)` in `[-90, 90]` degrees.
/// @see asin, sind, acosd
Value asind(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse cosine in degrees (`y = acosd(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `acosd(x)` in `[0, 180]` degrees.
/// @see acos, cosd, asind
Value acosd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse tangent in degrees (`y = atand(x)`).
///
/// @param x Input array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array of same shape containing `atand(x)` in `[-90, 90]` degrees.
/// @see atan, tand, atan2d
Value atand(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Four-quadrant inverse tangent in degrees (`z = atan2d(y, x)`).
///
/// @param y Y coordinates.
/// @param x X coordinates.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Broadcast array containing `atan2d(y, x)` in `[-180, 180]` degrees.
/// @see atan2, atand
Value atan2d(const Value &y, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise secant in degrees (`y = secd(x)`).
Value secd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise cosecant in degrees (`y = cscd(x)`).
Value cscd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise cotangent in degrees (`y = cotd(x)`).
Value cotd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse secant in degrees (`y = asecd(x)`).
Value asecd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse cosecant in degrees (`y = acscd(x)`).
Value acscd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise inverse cotangent in degrees (`y = acotd(x)`).
Value acotd(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise sine of pi times input (`y = sin(pi * x)`).
///
/// Computes `sin(pi * x)` with exact zeros at all integer inputs.
Value sinpi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise cosine of pi times input (`y = cos(pi * x)`).
///
/// Computes `cos(pi * x)` with exact zeros at odd half-integers.
Value cospi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts angles from degrees to radians (`y = deg2rad(x)`).
Value deg2rad(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts angles from radians to degrees (`y = rad2deg(x)`).
Value rad2deg(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Wraps angles in radians to `[-pi, pi]`.
Value wrapToPi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Wraps angles in radians to `[0, 2*pi)`.
Value wrapTo2Pi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Wraps angles in degrees to `[-180, 180]`.
Value wrapTo180(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Wraps angles in degrees to `[0, 360)`.
Value wrapTo360(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Hyperbolic Functions ────────────────────────────────────────────────────

/// @brief Hyperbolic sine (`y = sinh(x)`).
Value sinh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic cosine (`y = cosh(x)`).
Value cosh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic tangent (`y = tanh(x)`).
Value tanh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic sine (`y = asinh(x)`).
Value asinh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic cosine (`y = acosh(x)`).
Value acosh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic tangent (`y = atanh(x)`).
Value atanh(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic secant (`y = sech(x) = 1 / cosh(x)`).
Value sech(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic cosecant (`y = csch(x) = 1 / sinh(x)`).
Value csch(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hyperbolic cotangent (`y = coth(x) = 1 / tanh(x)`).
Value coth(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic secant (`y = asech(x)`).
Value asech(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic cosecant (`y = acsch(x)`).
Value acsch(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse hyperbolic cotangent (`y = acoth(x)`).
Value acoth(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Coordinate Transforms ───────────────────────────────────────────────────

/// @brief Result of 2D Cartesian to polar conversion.
struct PolarPair {
    Value theta;
    Value rho;
};

/// @brief Result of 3D Cartesian to cylindrical conversion.
struct CylTriple {
    Value theta;
    Value rho;
    Value z;
};

/// @brief Result of 2D polar to Cartesian conversion.
struct CartPair {
    Value x;
    Value y;
};

/// @brief Result of 3D cylindrical/spherical to Cartesian conversion.
struct CartTriple {
    Value x;
    Value y;
    Value z;
};

/// @brief Result of 3D Cartesian to spherical conversion.
struct SphTriple {
    Value az;
    Value el;
    Value r;
};

/// @brief Transforms 2D Cartesian coordinates to polar coordinates.
PolarPair cart2pol(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 3D Cartesian coordinates to cylindrical coordinates.
CylTriple cart2pol(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 2D polar coordinates to Cartesian coordinates.
CartPair pol2cart(const Value &theta, const Value &rho, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 3D cylindrical coordinates to Cartesian coordinates.
CartTriple pol2cart(const Value &theta, const Value &rho, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 3D Cartesian coordinates to spherical coordinates.
SphTriple cart2sph(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Transforms 3D spherical coordinates to Cartesian coordinates.
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
Value exp(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes `exp(x) - 1` accurately for small `x`.
Value expm1(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Natural logarithm (`ln(x)`).
Value log(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Base-10 logarithm (`log10(x)`).
Value log10(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Base-2 logarithm (`log2(x)`).
Value log2(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes `log(1 + x)` accurately for small `x`.
Value log1p(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Base-2 power scaling (`y = 2.^x`).
/// @param x Power exponent.
/// @param mr Memory resource.
/// @return Array of `2.^x`.
Value pow2(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Base-2 power scaling with mantissa and exponent (`y = f .* 2.^e`).
/// @param f Mantissa array.
/// @param e Exponent array.
/// @param mr Memory resource.
/// @return Scaled values `f .* 2.^e`.
Value pow2(const Value &f, const Value &e, std::pmr::memory_resource *mr = nullptr);

/// @brief Next higher power of 2 exponent (`p = nextpow2(n)`).
Value nextpow2(const Value &n, std::pmr::memory_resource *mr = nullptr);

/// @brief Square root (`y = sqrt(x)`).
Value sqrt(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Real square root with domain error check (`y = realsqrt(x)`).
Value realsqrt(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Real natural logarithm with domain error check (`y = reallog(x)`).
Value reallog(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cube root (`y = cbrt(x)`).
Value cbrt(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Real nth root of real numbers (`y = nthroot(x, n)`).
Value nthroot(const Value &x, const Value &n, std::pmr::memory_resource *mr = nullptr);

/// @brief Square root of sum of squares (`sqrt(x^2 + y^2)`).
Value hypot(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise power (`x .^ y`).
Value pow(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Real power with complex error check.
Value realpow(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

// ── Complex Numbers ─────────────────────────────────────────────────────────

/// @brief Real part of complex array (`y = real(x)`).
Value real(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Imaginary part of complex array (`y = imag(x)`).
Value imag(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Complex conjugate (`y = conj(x)`).
Value conj(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Phase angle of complex numbers in radians (`y = angle(x)`).
Value angle(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Absolute value / complex magnitude (`y = abs(x)`).
Value abs(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Constructs complex array from real part (`complex(re)`).
/// @param re Real components (imaginary set to zero).
/// @param mr Memory resource.
/// @return Complex array `re + 0i`.
Value complex(const Value &re, std::pmr::memory_resource *mr = nullptr);

/// @brief Constructs complex array from real and imaginary parts (`complex(re, im)`).
/// @param re Real components.
/// @param im Imaginary components.
/// @param mr Memory resource.
/// @return Complex array `re + im * 1i`.
Value complex(const Value &re, const Value &im, std::pmr::memory_resource *mr = nullptr);

/// @brief Signum function (-1, 0, 1 or unit complex phasor).
Value sign(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Unwraps phase angles across array dimensions.
Value unwrap(const Value &p, double tol = 3.14159265358979323846, int dim = -1, std::pmr::memory_resource *mr = nullptr);

/// @brief True for real array (false if complex storage is active).
Value isreal(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Rounding and Remainder ──────────────────────────────────────────────────

/// @brief Rounds elements to the nearest integer.
/// @param x Input array.
/// @param mr Memory resource.
/// @return Rounded elements.
Value round(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Rounds elements to N decimal digits or N significant digits (`round(x, n, type)`).
/// @param x Input array.
/// @param n Number of decimal or significant digits.
/// @param significant True to round to significant digits, false for decimals.
/// @param mr Memory resource.
/// @return Rounded elements.
Value roundN(const Value &x, int n, bool significant = false, std::pmr::memory_resource *mr = nullptr);

/// @brief Rounds down to the nearest integer (`y = floor(x)`).
Value floor(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Rounds up to the nearest integer (`y = ceil(x)`).
Value ceil(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Rounds towards zero / truncate (`y = fix(x)`).
Value fix(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Positive part / rectifier (`y = max(x, 0)`).
Value subplus(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Modulo operation where sign matches divisor (`y = mod(x, y)`).
Value mod(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Remainder after division where sign matches dividend (`y = rem(x, y)`).
Value rem(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
