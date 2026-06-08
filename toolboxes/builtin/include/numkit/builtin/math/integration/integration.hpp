// toolboxes/builtin/include/numkit/builtin/math/integration/integration.hpp
//
// Numerical integration / differentiation builtins.

#pragma once

#include <memory_resource>
#include <numkit/value/fn_handle.hpp>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::builtin {

/// @brief Finite-difference gradient (`g = gradient(F, h)`).
///
/// Central differences in the interior, one-sided at the endpoints.
/// - 1-D vector input → 1-D gradient.
/// - 2-D matrix input → `∂F/∂x` (along dim-2, columns).
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
/// `Fx = ∂F/∂x` (dim-2), `Fy = ∂F/∂y` (dim-1), x-direction first.
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

/// @brief Discrete Laplacian (`L = del2(U, h)`).
///
/// Approximates `∇²U / (2·ndims)` — MATLAB's `del2`. For each dimension a
/// centered second difference is taken on the interior (divided by `2h²`)
/// and linearly extrapolated to the boundary; the per-dimension terms are
/// summed and divided by the number of dimensions (always 2 here, since a
/// vector's `ndims` is 2). So a 1-D vector gives `½·(second difference)/h²`
/// and a 2-D matrix gives `¼·(Uxx + Uyy)/h²`.
///
/// v1 scope: 1-D vector and 2-D matrix inputs with a single scalar spacing
/// `h`. Per-axis spacing (`hx, hy`) and coordinate-vector spacing are a
/// documented gap.
///
/// @param u   1-D vector or 2-D matrix.
/// @param h   Uniform grid spacing (default 1, must be positive).
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE Laplacian, same shape as `U`.
/// @see gradient
Value del2(const Value &u, double h = 1.0,
           std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative trapezoidal integration with unit spacing
/// (`c = cumtrapz(y)`).
///
/// Vectors preserve shape; matrices integrate down each column
/// (first non-singleton dim).
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
/// The callback receives a 1-element `args` (the scalar evaluation
/// point) and writes its scalar result into `outs[0]`.
///
/// @param fn      Callback (scalar in, scalar out).
/// @param a       Lower limit.
/// @param b       Upper limit.
/// @param absTol  Absolute tolerance (default 1e-10 in adapter).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Scalar integral value.
/// @see trapz
Value integral(FnHandle fn, double a, double b, double absTol,
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
