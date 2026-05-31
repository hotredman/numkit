// libs/builtin/include/numkit/builtin/math/arithmetic/misc.hpp
//
// Miscellaneous elementary-math builtins that don't naturally group
// under trigonometry / exponents / rounding.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// @brief Degrees → radians (`y = deg2rad(x)`).
///
/// `y = x · π / 180`. Elementwise; preserves shape and (for complex
/// input) the imaginary part is scaled too.
///
/// @param x   Angle(s) in degrees.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Angle(s) in radians, same shape as `x`.
/// @see rad2deg
Value deg2rad(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Radians → degrees (`y = rad2deg(x)`).
///
/// `y = x · 180 / π`.
///
/// @param x   Angle(s) in radians.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Angle(s) in degrees, same shape as `x`.
/// @see deg2rad
Value rad2deg(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Wrap angles in radians to [-π, π] (`y = wrapToPi(x)`).
///
/// Values already in the closed interval are returned unchanged (so
/// `wrapToPi(π)=π`, `wrapToPi(-π)=-π`); values strictly outside are
/// wrapped via `wrapTo2Pi(x+π)-π`. Elementwise.
///
/// @param x   Angle(s) in radians.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Wrapped angle(s), same shape as `x`.
/// @see wrapTo2Pi, wrapTo180
Value wrapToPi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Wrap angles in radians to [0, 2π] (`y = wrapTo2Pi(x)`).
///
/// Equivalent to `mod(x, 2π)`, except a strictly positive input landing
/// exactly on 0 maps to 2π (so `wrapTo2Pi(2π)=2π`, but `wrapTo2Pi(0)=0`
/// and `wrapTo2Pi(-2π)=0`). Elementwise.
///
/// @param x   Angle(s) in radians.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Wrapped angle(s), same shape as `x`.
/// @see wrapToPi, wrapTo360
Value wrapTo2Pi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Wrap angles in degrees to [-180, 180] (`y = wrapTo180(x)`).
///
/// Degree-domain analogue of @ref wrapToPi: closed endpoints preserved,
/// values strictly outside wrapped via `wrapTo360(x+180)-180`.
/// Elementwise.
///
/// @param x   Angle(s) in degrees.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Wrapped angle(s), same shape as `x`.
/// @see wrapTo360, wrapToPi
Value wrapTo180(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Wrap angles in degrees to [0, 360] (`y = wrapTo360(x)`).
///
/// Degree-domain analogue of @ref wrapTo2Pi: `mod(x, 360)`, with a
/// strictly positive input landing on 0 mapped to 360 (so
/// `wrapTo360(360)=360`, but `wrapTo360(0)=0`). Elementwise.
///
/// @param x   Angle(s) in degrees.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Wrapped angle(s), same shape as `x`.
/// @see wrapTo180, wrapTo2Pi
Value wrapTo360(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Modulo with sign of divisor (`y = mod(a, b)`).
///
/// `y = a - floor(a / b) · b`. Result has the same sign as `b` (or is
/// 0). Distinct from @ref rem (which takes the sign of the dividend).
/// Broadcasts elementwise.
///
/// @param a   Dividend.
/// @param b   Divisor.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `a mod b`, broadcast shape.
/// @see rem
Value mod(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief IEEE remainder with sign of dividend (`y = rem(a, b)`).
///
/// `y = std::fmod(a, b)`. Result has the same sign as `a` (or is 0).
/// Broadcasts elementwise.
///
/// @param a   Dividend.
/// @param b   Divisor.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `a rem b`, broadcast shape.
/// @see mod
Value rem(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Euclidean norm of two reals (`y = hypot(x, y)`).
///
/// `sqrt(x² + y²)` computed without intermediate overflow
/// (via `std::hypot`). Broadcasts elementwise.
///
/// @param x   First leg.
/// @param y   Second leg.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `sqrt(x² + y²)`, broadcast shape.
Value hypot(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Real n-th root (`y = nthroot(x, n)`).
///
/// Real-valued n-th root: negative `x` with odd `n` produces a
/// negative real (unlike `x .^ (1/n)` which goes complex).
///
/// @param x   Radicand.
/// @param n   Root index (must be a positive number; integer-valued
///            and odd allowed for negative `x`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Real n-th root, broadcast shape.
Value nthroot(const Value &x, const Value &n, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
