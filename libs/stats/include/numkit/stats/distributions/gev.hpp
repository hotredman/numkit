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

/// GEV density (`y = gevpdf(x, k, sigma, mu)`).
///
/// Three sub-families depending on the shape `k`:
///   - `k > 0`: Fréchet (heavy-tailed).
///   - `k = 0`: Gumbel-for-maxima (limit case).
///   - `k < 0`: Reverse Weibull (bounded above).
Value gevpdf(const Value &x, double k, double sigma, double mu,
             std::pmr::memory_resource *mr = nullptr);

/// GEV CDF (`p = gevcdf(x, k, sigma, mu)`).
Value gevcdf(const Value &x, double k, double sigma, double mu,
             std::pmr::memory_resource *mr = nullptr);

/// GEV inverse CDF (`x = gevinv(p, k, sigma, mu)`).
Value gevinv(const Value &p, double k, double sigma, double mu,
             std::pmr::memory_resource *mr = nullptr);

/// GEV random samples (`r = gevrnd(k, sigma, mu, rows, cols)`).
Value gevrnd(double k, double sigma, double mu,
             size_t rows = 1, size_t cols = 1,
             std::pmr::memory_resource *mr = nullptr);

/// GEV mean / variance (`[m, v] = gevstat(k, sigma, mu)`).
///
/// Defined only for `k < 1` (mean) and `k < 1/2` (variance); falls
/// back to NaN outside those ranges.
std::tuple<double, double>
gevstat(double k, double sigma, double mu);

} // namespace numkit::stats
