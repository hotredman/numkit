// libs/stats/include/numkit/stats/distributions/lognormal.hpp
//
// Lognormal distribution: log(X) ~ N(μ, σ²). MATLAB convention parameterizes
// by μ, σ of the underlying normal (NOT by the mean / std of X itself).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// Lognormal density (`y = lognpdf(x, mu, sigma)`).
///
/// `log(X) ~ Normal(mu, sigma²)`. `mu` and `sigma` describe the
/// underlying normal — NOT the mean/std of X itself.
Value lognpdf(const Value &x, double mu, double sigma,
              std::pmr::memory_resource *mr = nullptr);

/// Lognormal CDF (`p = logncdf(x, mu, sigma)`).
Value logncdf(const Value &x, double mu, double sigma,
              std::pmr::memory_resource *mr = nullptr);

/// Lognormal inverse CDF (`x = logninv(p, mu, sigma)`).
Value logninv(const Value &p, double mu, double sigma,
              std::pmr::memory_resource *mr = nullptr);

/// Lognormal random samples (`r = lognrnd(mu, sigma, rows, cols)`).
Value lognrnd(double mu, double sigma, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// Lognormal mean and variance (`[m, v] = lognstat(mu, sigma)`).
///
/// `m = exp(mu + sigma²/2)`, `v = exp(2mu + sigma²)·(exp(sigma²) − 1)`.
std::tuple<double, double> lognstat(double mu, double sigma);

} // namespace numkit::stats
