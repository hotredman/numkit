// libs/stats/include/numkit/stats/distributions/extreme_value.hpp
//
// Type-I extreme value (Gumbel for minima) distribution.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Type-I EV (Gumbel-for-minima) pdf (`y = evpdf(x, mu, sigma)`).
///
/// @f$ f(x; \mu, \sigma) = \dfrac{1}{\sigma}\,e^{t}\,e^{-e^{t}},
///     \ t = (x - \mu)/\sigma @f$.
/// MATLAB parameterisation (`gumbel for minima`, NOT for maxima).
///
/// @param x      Evaluation points (any shape).
/// @param mu     Location parameter.
/// @param sigma  Scale parameter (`sigma > 0`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of pdf values, same shape as `x`.
/// @see evcdf, evinv, evrnd, evstat
Value evpdf(const Value &x, double mu, double sigma,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Type-I EV cdf (`p = evcdf(x, mu, sigma)`).
///
/// Closed form: @f$ F(x) = 1 - e^{-e^{t}},\ t = (x - \mu)/\sigma @f$.
///
/// @param x      Evaluation points (any shape).
/// @param mu     Location parameter.
/// @param sigma  Scale parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of cdf values in `[0, 1]`.
/// @see evpdf, evinv
Value evcdf(const Value &x, double mu, double sigma,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Type-I EV inverse cdf (`x = evinv(p, mu, sigma)`).
///
/// Closed form: `x = mu + sigma · log(-log(1 - p))`.
///
/// @param p      Probability levels in `[0, 1]` (any shape).
/// @param mu     Location parameter.
/// @param sigma  Scale parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Quantile array, same shape as `p`.
/// @see evcdf
Value evinv(const Value &p, double mu, double sigma,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Type-I EV random samples (`r = evrnd(mu, sigma, rows, cols)`).
///
/// Generated via inverse-CDF sampling.
///
/// @param mu     Location parameter.
/// @param sigma  Scale parameter.
/// @param rows   Output rows (default 1).
/// @param cols   Output columns (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `rows × cols` matrix of EV samples.
/// @see evpdf
Value evrnd(double mu, double sigma, size_t rows = 1, size_t cols = 1,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Type-I EV mean and variance (`[m, v] = evstat(mu, sigma)`).
///
/// Closed form: `m = mu - sigma · γ_E`, `v = sigma² · π²/6`
/// (γ_E ≈ 0.57721566490153286 is the Euler-Mascheroni constant).
///
/// @param mu     Location parameter.
/// @param sigma  Scale parameter.
/// @return       `{mean, variance}` pair.
/// @see evpdf
std::tuple<double, double> evstat(double mu, double sigma);

} // namespace numkit::stats
