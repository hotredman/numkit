// libs/builtin/include/numkit/builtin/math/integration/integration.hpp
//
// Numerical integration / differentiation builtins.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit { class Engine; }

namespace numkit::builtin {

using ::numkit::Engine;

/// @brief Finite-difference gradient (`g = gradient(F, h)`).
///
/// Central differences in the interior, one-sided at the endpoints.
/// - 1-D vector input → 1-D gradient.
/// - 2-D matrix input → `∂F/∂x` (along dim-2, columns; MATLAB convention).
///
/// @param f   Input array.
/// @param h   Uniform spacing (default 1).
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE gradient, same shape as `F`.
/// @see gradient2
Value gradient(const Value &f, double h = 1.0,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Two-direction gradient (`[Fx, Fy] = gradient2(F, hx, hy)`).
///
/// `Fx = ∂F/∂x` (dim-2), `Fy = ∂F/∂y` (dim-1) — MATLAB ordering with
/// x-direction first.
///
/// @param f   2-D input matrix.
/// @param hx  Spacing along columns (default 1).
/// @param hy  Spacing along rows (default 1).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(Fx, Fy)` pair, each same shape as `F`.
/// @see gradient
std::tuple<Value, Value>
gradient2(const Value &f, double hx = 1.0, double hy = 1.0,
          std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative trapezoidal integration with unit spacing
/// (`c = cumtrapz(y)`).
///
/// Vectors preserve shape; matrices integrate down each column
/// (first non-singleton dim, MATLAB default).
///
/// @param y   Integrand values.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Cumulative integral, same shape as `y`.
/// @see cumtrapz(x, y, mr), trapz
Value cumtrapz(const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative trapezoidal integration with explicit spacing
/// (`c = cumtrapz(x, y)`).
///
/// For matrix `y`, `x` may be a column-length vector (broadcast per
/// column) or a matrix of the same shape (per-column spacing).
///
/// @param x   Sample sites.
/// @param y   Integrand values.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Cumulative integral, same shape as `y`.
/// @see cumtrapz(y, mr)
Value cumtrapz(const Value &x, const Value &y,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Adaptive Gauss-Kronrod definite integral
/// (`I = integral(fn, a, b, absTol)`).
///
/// 15-point Kronrod with embedded 7-point Gauss. Recurses on
/// subintervals where `|G - K|` exceeds `absTol`. Up to ~16 subdivision
/// levels per branch.
///
/// @param fn      Function handle (single-argument, returns scalar).
/// @param a       Lower limit.
/// @param b       Upper limit.
/// @param absTol  Absolute tolerance (default 1e-10 in adapter).
/// @param engine  Engine context for function-handle invocation.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Scalar integral value.
/// @see trapz
Value integral(const Value &fn, double a, double b, double absTol,
               Engine *engine,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Trapezoidal integration with unit spacing (`I = trapz(y)`).
///
/// @param y   Integrand values.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar integral (vector input) or row reduced from
///            columns (matrix input).
/// @see trapz(x, y, mr), cumtrapz
Value trapz(const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Trapezoidal integration with explicit spacing
/// (`I = trapz(x, y)`).
///
/// @param x   Sample sites (length must match `numel(y)`).
/// @param y   Integrand values.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Integral value.
/// @throws Error  `numel(x) != numel(y)` (`m:trapz:badLen`).
Value trapz(const Value &x, const Value &y,
            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
