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

/// normpdf(x[, mu[, sigma]]) — probability density.
Value normpdf(const Value &x, double mu = 0.0, double sigma = 1.0, std::pmr::memory_resource *mr = nullptr);

/// normcdf(x[, mu[, sigma]]) — cumulative distribution.
Value normcdf(const Value &x, double mu = 0.0, double sigma = 1.0, std::pmr::memory_resource *mr = nullptr);

/// norminv(p[, mu[, sigma]]) — inverse CDF (quantile function).
Value norminv(const Value &p, double mu = 0.0, double sigma = 1.0, std::pmr::memory_resource *mr = nullptr);

/// normrnd(mu, sigma[, m[, n]]) — random samples. Without m/n returns
/// a scalar; with m, n returns m × n.
Value normrnd(double mu, double sigma, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// normstat(mu, sigma) — return [mean, variance].
std::tuple<double, double> normstat(double mu, double sigma);

} // namespace numkit::stats
