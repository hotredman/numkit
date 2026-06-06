// libs/builtin/include/numkit/builtin/math/exp_log/exponents.hpp
//
// Exponentials and logarithms.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::builtin {

/// @file
/// @brief Exponentials and logarithms.
///
/// `exp` and `log` are SIMD-backed (Highway); the rest are scalar
/// wrappers around `<cmath>`. The internal `hint` overload (reuses a
/// uniquely-owned heap buffer as the result) is intentionally hidden
/// from this public header — see `libs/builtin/src/math/_unary_hint.hpp`.

/// @brief Square root (`y = sqrt(x)`).
///
/// Elementwise. For negative real input returns NaN
/// (use @ref realsqrt to throw instead, or @ref complex first to get
/// a COMPLEX result).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Square root, same shape as `x`.
/// @see realsqrt, pow2
Value sqrt(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Natural exponential (`y = exp(x)`).
///
/// SIMD-backed. Elementwise.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `e^x`, same shape as `x`.
/// @see log, expm1
Value exp(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Natural logarithm (`y = log(x)`).
///
/// SIMD-backed. Negative real input returns NaN (use @ref reallog
/// for an error-throwing variant).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `ln(x)`, same shape as `x`.
/// @see exp, log2, log10, log1p
Value log(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Base-2 logarithm (`y = log2(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `log2(x)`, same shape as `x`.
/// @see log, log10
Value log2(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Base-10 logarithm (`y = log10(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `log10(x)`, same shape as `x`.
/// @see log, log2
Value log10(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Accurate `e^x - 1` (`y = expm1(x)`).
///
/// Avoids catastrophic cancellation near zero (where `exp(x) - 1`
/// would lose precision).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `e^x - 1`, same shape as `x`.
/// @see exp, log1p
Value expm1(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Accurate `log(1 + x)` (`y = log1p(x)`).
///
/// Avoids catastrophic cancellation near zero.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `log(1 + x)`, same shape as `x`.
/// @see log, expm1
Value log1p(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Powers of two (`y = pow2(y)`).
///
/// Elementwise `2 .^ y`.
///
/// @param y   Exponents.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `2^y`, same shape as `y`.
/// @see pow2(f, e, mr)
Value pow2(const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Scaled powers of two (`y = pow2(f, e)`).
///
/// Elementwise `f .* 2 .^ e` via `libc ldexp` (exact mantissa scaling).
///
/// @param f   Mantissas.
/// @param e   Exponents (broadcasts elementwise).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `f · 2^e`, broadcast shape.
/// @see pow2(y, mr)
Value pow2(const Value &f, const Value &e, std::pmr::memory_resource *mr = nullptr);

/// @brief Real-domain power (`y = realpow(x, y)`).
///
/// Errors if any result would be complex (negative base with
/// non-integer exponent).
///
/// @param x   Base array.
/// @param y   Exponent array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `x .^ y`, broadcast shape.
/// @throws Error  Result would be complex (`m:realpow:complexResult`).
/// @see pow2
Value realpow(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Real-domain logarithm (`y = reallog(x)`).
///
/// Errors on `x < 0` (which would yield a complex result).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `ln(x)`, same shape as `x`.
/// @throws Error  Negative input (`m:reallog:negInput`).
/// @see log
Value reallog(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Real-domain square root (`y = realsqrt(x)`).
///
/// Errors on `x < 0`.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `sqrt(x)`, same shape as `x`.
/// @throws Error  Negative input (`m:realsqrt:negInput`).
/// @see sqrt
Value realsqrt(const Value &x, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
