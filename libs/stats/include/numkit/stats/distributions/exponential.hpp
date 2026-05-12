// libs/stats/include/numkit/stats/distributions/exponential.hpp
//
// Exponential distribution. MATLAB parameterization uses the MEAN μ
// (NOT the rate λ): f(x; μ) = exp(-x/μ) / μ. Special case Gamma(1, μ).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// exppdf(x, mu) — pdf at x with mean μ > 0.
Value exppdf(const Value &x, double mu, std::pmr::memory_resource *mr = nullptr);

/// expcdf(x, mu) — F(x) = 1 - exp(-x/μ).
Value expcdf(const Value &x, double mu, std::pmr::memory_resource *mr = nullptr);

/// expinv(p, mu) — F^{-1}(p) = -μ · log(1 - p).
Value expinv(const Value &p, double mu, std::pmr::memory_resource *mr = nullptr);

/// exprnd(mu[, m, n]) — std::exponential_distribution with rate 1/μ.
Value exprnd(double mu, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// expstat(mu) — mean = μ, variance = μ².
std::tuple<double, double> expstat(double mu);

} // namespace numkit::stats
