// include/numkit/builtin/polyfun.hpp
//
// Polynomials, interpolation, integration, and piecewise polynomials.
#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>

namespace numkit {
class Engine;
}

namespace numkit::builtin {

/// @file
/// @brief Polynomials, interpolation, integration, and piecewise curves.

// ── Polynomials ─────────────────────────────────────────────────────────────

/// @brief Polynomial roots.
Value roots(const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial with specified roots or characteristic polynomial.
Value poly(const Value &r, std::pmr::memory_resource *mr = nullptr);

/// @brief Evaluates polynomial `p` at points `x`.
Value polyval(const Value &p, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial derivative.
Value polyder(const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial integration with integration constant `k`.
Value polyint(const Value &p, double k = 0.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial curve fitting of degree `n`.
Value polyfit(const Value &x, const Value &y, size_t n, std::pmr::memory_resource *mr = nullptr);

// ── Interpolation & Piecewise Polynomials ───────────────────────────────────

/// @brief 1-D data interpolation.
Value interp1(const Value &x, const Value &v, const Value &xq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D data interpolation on a grid.
Value interp2(const Value &x, const Value &y, const Value &v, const Value &xq, const Value &yq, const std::string &method = "linear", std::pmr::memory_resource *mr = nullptr);

/// @brief Cubic spline interpolation.
Value spline(const Value &x, const Value &y, const Value &xq = Value(), std::pmr::memory_resource *mr = nullptr);

/// @brief Piecewise Cubic Hermite Interpolating Polynomial (PCHIP).
Value pchip(const Value &x, const Value &y, const Value &xq = Value(), std::pmr::memory_resource *mr = nullptr);

/// @brief Constructs a piecewise polynomial structure.
Value mkpp(const Value &breaks, const Value &coefs, std::pmr::memory_resource *mr = nullptr);

/// @brief Extracts piecewise polynomial structure fields.
Value unmkpp(const Value &pp, std::pmr::memory_resource *mr = nullptr);

/// @brief Evaluates a piecewise polynomial structure at query points `xq`.
Value ppval(const Value &pp, const Value &xq, std::pmr::memory_resource *mr = nullptr);

// ── Numerical Integration ───────────────────────────────────────────────────

/// @brief Trapezoidal numerical integration with unit spacing.
Value trapz(const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Trapezoidal numerical integration with coordinate array `x`.
Value trapz(const Value &x, const Value &y, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative trapezoidal numerical integration.
Value cumtrapz(const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief Cumulative trapezoidal numerical integration with coordinate array `x`.
Value cumtrapz(const Value &x, const Value &y, int dim = 0, std::pmr::memory_resource *mr = nullptr);

// ── Registration ────────────────────────────────────────────────────────────

/// @brief Registers all polynomial and calculus builtins into the engine instance.
void register_polyfun(Engine &engine);

} // namespace numkit::builtin
