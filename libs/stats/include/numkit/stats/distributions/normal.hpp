// libs/stats/include/numkit/stats/distributions/normal.hpp
//
// Normal (Gaussian) distribution functions: pdf / cdf / icdf / rnd /
// stat.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Normal pdf (`y = normpdf(x, mu, sigma)`).
///
/// @f$ f(x; \mu, \sigma) = \dfrac{1}{\sigma\sqrt{2\pi}}\,
///     e^{-(x-\mu)^2 / (2\sigma^2)} @f$.
/// Defaults give the standard normal `N(0, 1)`.
///
/// @param x      Evaluation points (any shape).
/// @param mu     Mean parameter (default 0).
/// @param sigma  Standard deviation (default 1, `sigma > 0`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of pdf values, same shape as `x`.
/// @see normcdf, norminv, normrnd, normstat
Value normpdf(const Value &x, double mu = 0.0, double sigma = 1.0,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Normal cdf (`p = normcdf(x, mu, sigma)`).
///
/// Closed form:
/// @f$ F(x; \mu, \sigma) = \tfrac{1}{2}\bigl[1 + \text{erf}\bigl((x-\mu)/(\sigma\sqrt{2})\bigr)\bigr] @f$.
///
/// @param x      Evaluation points (any shape).
/// @param mu     Mean parameter (default 0).
/// @param sigma  Standard deviation (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of cdf values in `[0, 1]`.
/// @see normpdf, norminv
Value normcdf(const Value &x, double mu = 0.0, double sigma = 1.0,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Normal inverse cdf / quantile (`x = norminv(p, mu, sigma)`).
///
/// `p` must be in `[0, 1]`; values outside the closed interval return
/// `NaN`.
///
/// @param p      Probability levels in `[0, 1]` (any shape).
/// @param mu     Mean parameter (default 0).
/// @param sigma  Standard deviation (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Quantile array, same shape as `p`.
/// @see normcdf
Value norminv(const Value &p, double mu = 0.0, double sigma = 1.0,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Normal random samples (`r = normrnd(mu, sigma, rows, cols)`).
///
/// @param mu     Mean parameter.
/// @param sigma  Standard deviation (`sigma > 0`).
/// @param rows   Output rows (default 1).
/// @param cols   Output columns (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `rows × cols` matrix of normal samples.
/// @see normpdf
Value normrnd(double mu, double sigma, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Normal mean and variance (`[m, v] = normstat(mu, sigma)`).
///
/// Trivially: `m = mu`, `v = sigma²`.
///
/// @param mu     Mean parameter.
/// @param sigma  Standard deviation.
/// @return       `{mean, variance}` pair.
/// @see normpdf
std::tuple<double, double> normstat(double mu, double sigma);

} // namespace numkit::stats
