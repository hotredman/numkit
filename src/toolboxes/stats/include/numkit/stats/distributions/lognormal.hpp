// toolboxes/stats/include/numkit/stats/distributions/lognormal.hpp
//
// Lognormal distribution: log(X) ~ N(μ, σ²).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Lognormal pdf (`y = lognpdf(x, mu, sigma)`).
///
/// @f$ f(x; \mu, \sigma) = \dfrac{1}{x\,\sigma\sqrt{2\pi}}\,
///     e^{-(\ln x - \mu)^2 / (2\sigma^2)} @f$ for `x > 0`.
/// Convention: `mu` / `sigma` describe the underlying normal
/// distribution `log(X) ~ N(mu, sigma²)`, NOT the mean/std of `X` itself.
///
/// @param x      Evaluation points (any shape).
/// @param mu     Mean of the underlying normal.
/// @param sigma  Standard deviation of the underlying normal (`sigma > 0`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of pdf values, same shape as `x`.
/// @see logncdf, logninv, lognrnd, lognstat
Value lognpdf(const Value &x, double mu, double sigma,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Lognormal cdf (`p = logncdf(x, mu, sigma)`).
///
/// Closed form: `F(x) = Φ((log(x) - mu) / sigma)` for `x > 0`.
///
/// @param x      Evaluation points (any shape).
/// @param mu     Mean of the underlying normal.
/// @param sigma  Standard deviation of the underlying normal.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of cdf values in `[0, 1]`.
/// @see lognpdf, logninv
Value logncdf(const Value &x, double mu, double sigma,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Lognormal inverse cdf (`x = logninv(p, mu, sigma)`).
///
/// Closed form: `x = exp(mu + sigma · Φ^{-1}(p))`.
///
/// @param p      Probability levels in `[0, 1]` (any shape).
/// @param mu     Mean of the underlying normal.
/// @param sigma  Standard deviation of the underlying normal.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Quantile array, same shape as `p`.
/// @see logncdf
Value logninv(const Value &p, double mu, double sigma,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Lognormal random samples (`r = lognrnd(mu, sigma, rows, cols)`).
///
/// Sampled as `exp(mu + sigma · randn())`.
///
/// @param mu     Mean of the underlying normal.
/// @param sigma  Standard deviation of the underlying normal.
/// @param rows   Output rows (default 1).
/// @param cols   Output columns (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `rows × cols` matrix of lognormal samples.
/// @see lognpdf
Value lognrnd(double mu, double sigma, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Lognormal mean and variance (`[m, v] = lognstat(mu, sigma)`).
///
/// Closed form:
/// `m = exp(mu + sigma²/2)`,
/// `v = exp(2·mu + sigma²) · (exp(sigma²) - 1)`.
///
/// @param mu     Mean of the underlying normal.
/// @param sigma  Standard deviation of the underlying normal.
/// @return       `{mean, variance}` pair (of the lognormal RV `X`, not the log).
/// @see lognpdf
std::tuple<double, double> lognstat(double mu, double sigma);

} // namespace numkit::stats
