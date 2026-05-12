// libs/stats/include/numkit/stats/distributions/gamma_dist.hpp
//
// Gamma distribution Gamma(a, b) — MATLAB convention: a = shape, b = scale,
// so f(x) = x^(a-1) exp(-x/b) / (b^a Γ(a)). cdf composes gammainc on x/b;
// icdf uses gammaincinv; rnd uses std::gamma_distribution directly.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// gampdf(x, a, b) — pdf at x with shape a > 0, scale b > 0.
Value gampdf(const Value &x, double a, double b, std::pmr::memory_resource *mr = nullptr);

/// gamcdf(x, a, b) — cdf via regularized lower incomplete gamma:
///   F(x) = P(a, x/b) = gammainc(x/b, a)
Value gamcdf(const Value &x, double a, double b, std::pmr::memory_resource *mr = nullptr);

/// gaminv(p, a, b) — inverse cdf via gammaincinv: x = b · P^{-1}(p; a).
Value gaminv(const Value &p, double a, double b, std::pmr::memory_resource *mr = nullptr);

/// gamrnd(a, b[, m, n]) — draws from Gamma(shape=a, scale=b).
Value gamrnd(double a, double b, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// gamstat(a, b) — mean = a·b, variance = a·b².
std::tuple<double, double> gamstat(double a, double b);

} // namespace numkit::stats
