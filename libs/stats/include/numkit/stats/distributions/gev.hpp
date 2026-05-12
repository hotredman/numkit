// libs/stats/include/numkit/stats/distributions/gev.hpp
//
// Generalized Extreme Value distribution. MATLAB convention:
// gev*(x, k, sigma, mu).
//   k > 0  → Frechet
//   k = 0  → Gumbel-MAX (limit)  pdf = (1/σ)·exp(-z)·exp(-exp(-z)),  z=(x-μ)/σ
//   k < 0  → Reverse Weibull (bounded above)
// For k ≠ 0, the standardised t = 1 + k·(x-μ)/σ must be > 0.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value gevpdf(const Value &x, double k, double sigma, double mu, std::pmr::memory_resource *mr = nullptr);
Value gevcdf(const Value &x, double k, double sigma, double mu, std::pmr::memory_resource *mr = nullptr);
Value gevinv(const Value &p, double k, double sigma, double mu, std::pmr::memory_resource *mr = nullptr);
Value gevrnd(double k, double sigma, double mu, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);
std::tuple<double, double>
gevstat(double k, double sigma, double mu);

} // namespace numkit::stats
