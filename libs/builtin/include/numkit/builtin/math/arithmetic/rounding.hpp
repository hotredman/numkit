// libs/builtin/include/numkit/builtin/math/arithmetic/rounding.hpp
//
// Rounding and sign builtins.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::builtin {

/// @brief Absolute value (`y = abs(x)`).
///
/// Elementwise. For COMPLEX input returns `sqrt(re² + im²)` per element.
/// Has a SIMD backend (Highway via `libs/builtin/src/backends/`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `|x|`, same shape as `x`.
/// @see sign
Value abs(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Round toward `-Inf` (`y = floor(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Floor of each element, same shape as `x`.
/// @see ceil, round, fix
Value floor(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Round toward `+Inf` (`y = ceil(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Ceiling of each element, same shape as `x`.
/// @see floor, round, fix
Value ceil(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Round to nearest integer (`y = round(x)`).
///
/// Ties round away from zero.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Nearest integer per element, same shape as `x`.
/// @see floor, ceil, fix
Value round(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Truncate toward zero (`y = fix(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Integer part toward zero, same shape as `x`.
/// @see round, floor, ceil
Value fix(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Sign function (`y = sign(x)`).
///
/// Returns `-1`, `0`, or `+1` per element. For COMPLEX input returns
/// `x / |x|` (unit-magnitude phasor) or 0 for zero entries.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Sign of each element, same shape as `x`.
/// @see abs
Value sign(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Positive part (`y = subplus(x)`).
///
/// `y = max(x, 0)`. NaN passes through unchanged.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Truncated positive part, same shape as `x`.
Value subplus(const Value &x, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
