// src/builtin/include/numkit/builtin/polyfun.hpp
//
// Pure C++ Polynomials, interpolation, integration, and piecewise polynomials.
#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @file
/// @brief Polynomials, interpolation, numerical integration, and piecewise polynomial curves.
///
/// Provides a clean, engine-free C++ API for polynomial arithmetic and root finding,
/// 1D/2D interpolation, spline and PCHIP curves, and trapezoidal integration.

// ── Polynomials ─────────────────────────────────────────────────────────────

/// @brief Computes polynomial roots (`roots(p)`).
/// @param p Vector of polynomial coefficients in descending powers: `p[0]*x^N + ... + p[N]`.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Column vector of complex/real roots (eigenvalues of companion matrix).
/// @see poly, polyval
Value roots(const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial with specified roots or characteristic polynomial of a matrix.
/// @param r Vector of roots or square matrix.
/// @param mr Memory resource.
/// @return Row vector of polynomial coefficients in descending powers.
/// @see roots, polyval
Value poly(const Value &r, std::pmr::memory_resource *mr = nullptr);

/// @brief Evaluates polynomial `p` at points `x` via Horner's scheme.
/// @param p Vector of polynomial coefficients.
/// @param x Evaluation point(s).
/// @param mr Memory resource.
/// @return Evaluated values with the same shape as @p x.
/// @see roots, polyder, polyint
Value polyval(const Value &p, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes polynomial derivative (`polyder(p)`).
/// @param p Vector of polynomial coefficients.
/// @param mr Memory resource.
/// @return Derivative polynomial coefficients.
/// @see polyint, polyval
Value polyder(const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief Computes polynomial integral (`polyint(p, k)`).
/// @param p Vector of polynomial coefficients.
/// @param k Constant of integration (default: 0.0).
/// @param mr Memory resource.
/// @return Integrated polynomial coefficients.
/// @see polyder, polyval
Value polyint(const Value &p, double k = 0.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial least-squares curve fitting (`polyfit(x, y, n)`).
/// @param x Independent variable vector.
/// @param y Dependent variable vector.
/// @param n Degree of the fitting polynomial.
/// @param mr Memory resource.
/// @return Fitted polynomial coefficients of length `n + 1`.
/// @see polyval
Value polyfit(const Value &x, const Value &y, size_t n, std::pmr::memory_resource *mr = nullptr);

// ── Interpolation & Piecewise Polynomials ───────────────────────────────────

/// @brief 1-D table lookup and data interpolation (`interp1(x, v, xq, method)`).
/// @param x Sample grid points (strictly monotonic).
/// @param v Sample values.
/// @param xq Query points.
/// @param method Interpolation method: `"linear"`, `"nearest"`, `"next"`, `"previous"`, `"spline"`, `"pchip"`.
/// @param mr Memory resource.
/// @return Interpolated values at query points `xq`.
/// @see interp2, spline, pchip
Value interp1(const Value &x, const Value &v, const Value &xq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D data interpolation on a grid (`interp2(x, y, v, xq, yq, method)`).
/// @param x Grid coordinates along columns.
/// @param y Grid coordinates along rows.
/// @param v Matrix of values on grid.
/// @param xq Query points x-coordinates.
/// @param yq Query points y-coordinates.
/// @param method Interpolation method: `"linear"`, `"nearest"`, `"cubic"`, `"spline"`.
/// @param mr Memory resource.
/// @return Interpolated values.
/// @see interp1
Value interp2(const Value &x, const Value &y, const Value &v, const Value &xq, const Value &yq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

/// @brief Cubic spline data interpolation.
/// @param x Sample grid points.
/// @param y Sample values.
/// @param xq Query points (optional, returns piecewise polynomial struct if empty).
/// @param mr Memory resource.
/// @return Interpolated values or ppform structure.
/// @see pchip, ppval, unmkpp
Value spline(const Value &x, const Value &y, const Value &xq = Value(), std::pmr::memory_resource *mr = nullptr);

/// @brief Piecewise Cubic Hermite Interpolating Polynomial (PCHIP - shape-preserving).
/// @param x Sample grid points.
/// @param y Sample values.
/// @param xq Query points (optional, returns piecewise polynomial struct if empty).
/// @param mr Memory resource.
/// @return Interpolated values or ppform structure.
/// @see spline, ppval
Value pchip(const Value &x, const Value &y, const Value &xq = Value(), std::pmr::memory_resource *mr = nullptr);

/// @brief Constructs a piecewise polynomial structure (ppform).
/// @param breaks Vector of break points.
/// @param coefs Matrix of local polynomial coefficients.
/// @param mr Memory resource.
/// @return Piecewise polynomial struct.
/// @see unmkpp, ppval
Value mkpp(const Value &breaks, const Value &coefs, std::pmr::memory_resource *mr = nullptr);

/// @brief Extracts piecewise polynomial structure fields (`[breaks, coefs, l, k, d] = unmkpp(pp)`).
/// @param pp Piecewise polynomial struct.
/// @param mr Memory resource.
/// @return Breaks vector from pp struct.
/// @see mkpp, ppval
Value unmkpp(const Value &pp, std::pmr::memory_resource *mr = nullptr);

/// @brief Evaluates a piecewise polynomial structure at query points `xq`.
/// @param pp Piecewise polynomial structure.
/// @param xq Query points.
/// @param mr Memory resource.
/// @return Evaluated values.
/// @see mkpp, unmkpp, spline, pchip
Value ppval(const Value &pp, const Value &xq, std::pmr::memory_resource *mr = nullptr);

// ── Numerical Integration ───────────────────────────────────────────────────

/// @brief Trapezoidal numerical integration with unit spacing (`trapz(y)`).
/// @param y Input array.
/// @param mr Memory resource.
/// @return Approximate integral.
/// @see cumtrapz
Value trapz(const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Trapezoidal numerical integration with coordinate array `x` (`trapz(x, y)`).
/// @param x Coordinate vector.
/// @param y Input array.
/// @param dim Dimension along which to integrate (0 = default).
/// @param mr Memory resource.
/// @return Approximate integral.
/// @see cumtrapz
Value trapz(const Value &x, const Value &y, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative trapezoidal numerical integration with unit spacing (`cumtrapz(y)`).
/// @param y Input array.
/// @param mr Memory resource.
/// @return Cumulative integral array.
/// @see trapz
Value cumtrapz(const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative trapezoidal numerical integration with coordinate array `x` (`cumtrapz(x, y)`).
/// @param x Coordinate vector.
/// @param y Input array.
/// @param dim Dimension along which to integrate (0 = default).
/// @param mr Memory resource.
/// @return Cumulative integral array.
/// @see trapz
Value cumtrapz(const Value &x, const Value &y, int dim = 0, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
