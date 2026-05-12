// libs/stats/include/numkit/stats/distributions/gev.hpp
//
// Generalized Extreme Value distribution.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief GEV pdf (`y = gevpdf(x, k, sigma, mu)`).
///
/// Three sub-families depending on the shape `k`:
/// - `k > 0` → Fréchet (heavy-tailed, lower bound `mu - sigma/k`)
/// - `k = 0` → Gumbel-for-maxima limit:
///   `f = (1/σ) e^{-z} e^{-e^{-z}}`, `z = (x-mu)/sigma`
/// - `k < 0` → Reverse Weibull (bounded above at `mu - sigma/k`)
///
/// For `k ≠ 0`, the standardised `t = 1 + k·(x-mu)/sigma` must be `> 0`.
///
/// @param x      Evaluation points (any shape).
/// @param k      Shape parameter (any real).
/// @param sigma  Scale parameter (`sigma > 0`).
/// @param mu     Location parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of pdf values, same shape as `x`.
/// @see gevcdf, gevinv, gevrnd, gevstat
Value gevpdf(const Value &x, double k, double sigma, double mu,
             std::pmr::memory_resource *mr = nullptr);

/// @brief GEV cdf (`p = gevcdf(x, k, sigma, mu)`).
///
/// @param x      Evaluation points (any shape).
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param mu     Location parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of cdf values in `[0, 1]`.
/// @see gevpdf, gevinv
Value gevcdf(const Value &x, double k, double sigma, double mu,
             std::pmr::memory_resource *mr = nullptr);

/// @brief GEV inverse cdf (`x = gevinv(p, k, sigma, mu)`).
///
/// @param p      Probability levels in `[0, 1]` (any shape).
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param mu     Location parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Quantile array, same shape as `p`.
/// @see gevcdf
Value gevinv(const Value &p, double k, double sigma, double mu,
             std::pmr::memory_resource *mr = nullptr);

/// @brief GEV random samples (`r = gevrnd(k, sigma, mu, rows, cols)`).
///
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param mu     Location parameter.
/// @param rows   Output rows (default 1).
/// @param cols   Output columns (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `rows × cols` matrix of GEV samples.
/// @see gevpdf
Value gevrnd(double k, double sigma, double mu,
             size_t rows = 1, size_t cols = 1,
             std::pmr::memory_resource *mr = nullptr);

/// @brief GEV mean and variance (`[m, v] = gevstat(k, sigma, mu)`).
///
/// Mean is defined only for `k < 1`; variance only for `k < 1/2`.
/// Returns `NaN` outside those ranges.
///
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param mu     Location parameter.
/// @return       `{mean, variance}` pair.
/// @see gevpdf
std::tuple<double, double>
gevstat(double k, double sigma, double mu);

} // namespace numkit::stats
