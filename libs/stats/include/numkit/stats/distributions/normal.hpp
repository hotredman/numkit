// libs/stats/include/numkit/stats/distributions/normal.hpp
//
// Normal (Gaussian) distribution functions: pdf / cdf / icdf / rnd /
// stat — MATLAB Statistics Toolbox parity. All accept optional
// (mu, sigma) parameters with defaults (0, 1) for the standard normal.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// Normal probability density (`y = normpdf(x, mu, sigma)`).
///
/// @f$ f(x; \mu, \sigma) = \frac{1}{\sigma\sqrt{2\pi}}\,\exp\!\left(-\frac{(x-\mu)^2}{2\sigma^2}\right) @f$.
///
/// @param x      Evaluation point(s) (any shape).
/// @param mu     Mean parameter (default 0).
/// @param sigma  Standard deviation (default 1, must be > 0).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Density values, same shape as `x`.
///
/// @see normcdf, norminv, normrnd
Value normpdf(const Value &x, double mu = 0.0, double sigma = 1.0,
              std::pmr::memory_resource *mr = nullptr);

/// Normal CDF (`p = normcdf(x, mu, sigma)`).
///
/// @f$ F(x; \mu, \sigma) = \tfrac{1}{2}\bigl[1 + \text{erf}\!\bigl((x-\mu)/(\sigma\sqrt 2)\bigr)\bigr] @f$.
Value normcdf(const Value &x, double mu = 0.0, double sigma = 1.0,
              std::pmr::memory_resource *mr = nullptr);

/// Normal inverse CDF / quantile (`x = norminv(p, mu, sigma)`).
///
/// `p` must be in [0, 1]; values outside the closed interval return
/// NaN (matches MATLAB).
Value norminv(const Value &p, double mu = 0.0, double sigma = 1.0,
              std::pmr::memory_resource *mr = nullptr);

/// Normal random samples (`r = normrnd(mu, sigma, rows, cols)`).
///
/// @param rows,cols  Output size (default 1×1 = scalar).
Value normrnd(double mu, double sigma, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// Normal mean and variance (`[m, v] = normstat(mu, sigma)`).
///
/// Returns `(mu, sigma²)`.
std::tuple<double, double> normstat(double mu, double sigma);

} // namespace numkit::stats
