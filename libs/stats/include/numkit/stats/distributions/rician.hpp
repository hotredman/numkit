// libs/stats/include/numkit/stats/distributions/rician.hpp
//
// Rician (Rice) distribution.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Rician (Rice) pdf (`y = ricepdf(x, s, sigma)`).
///
/// @f$ f(x; s, \sigma) = (x/\sigma^2)\,e^{-(x^2 + s^2)/(2\sigma^2)}\,
///     I_0(x s / \sigma^2) @f$ for `x >= 0`.
/// Special case: `s = 0` reduces to @ref raylpdf.
///
/// @param x      Evaluation points (any shape).
/// @param s      Noncentrality / offset (`s >= 0`).
/// @param sigma  Scale parameter (`sigma > 0`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of pdf values, same shape as `x`.
/// @see ricecdf, riceinv, ricernd, ricestat, raylpdf
Value ricepdf(const Value &x, double s, double sigma,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Rician cdf (`p = ricecdf(x, s, sigma)`).
///
/// Computed via the Marcum Q-function:
/// @f$ F(x) = 1 - Q_1(s/\sigma,\ x/\sigma) @f$.
///
/// @param x      Evaluation points (any shape).
/// @param s      Noncentrality parameter.
/// @param sigma  Scale parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of cdf values in `[0, 1]`.
/// @see ricepdf, riceinv
Value ricecdf(const Value &x, double s, double sigma,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Rician inverse cdf (`x = riceinv(p, s, sigma)`).
///
/// @param p      Probability levels in `[0, 1]` (any shape).
/// @param s      Noncentrality parameter.
/// @param sigma  Scale parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Quantile array, same shape as `p`.
/// @see ricecdf
Value riceinv(const Value &p, double s, double sigma,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Rician random samples (`r = ricernd(s, sigma, rows, cols)`).
///
/// Generated as `sqrt(N1² + N2²)` with `N1 ~ N(s, σ²)`, `N2 ~ N(0, σ²)`.
///
/// @param s      Noncentrality parameter.
/// @param sigma  Scale parameter.
/// @param rows   Output rows (default 1).
/// @param cols   Output columns (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `rows × cols` matrix of Rician samples.
/// @see ricepdf
Value ricernd(double s, double sigma, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Rician mean and variance (`[m, v] = ricestat(s, sigma)`).
///
/// Expressed via the Laguerre polynomial `L_{1/2}` (or equivalently
/// the confluent hypergeometric `_1F_1`). Returns `{mean, variance}`
/// computed numerically.
///
/// @param s      Noncentrality parameter.
/// @param sigma  Scale parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `{mean, variance}` pair.
/// @see ricepdf
std::tuple<double, double>
ricestat(double s, double sigma,
         std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
