// libs/stats/include/numkit/stats/distributions/weibull.hpp
//
// Weibull distribution. MATLAB convention: a = scale, b = shape, so
// f(x) = (b/a)·(x/a)^(b-1)·exp(-(x/a)^b). Note std::weibull_distribution
// takes (shape, scale) — opposite order.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// Weibull density (`y = wblpdf(x, a, b)`).
///
/// @f$ f(x; a, b) = (b/a)\,(x/a)^{b-1}\,e^{-(x/a)^b} @f$ for x ≥ 0.
/// MATLAB convention: `a = scale`, `b = shape`.
Value wblpdf(const Value &x, double a, double b,
             std::pmr::memory_resource *mr = nullptr);

/// Weibull CDF (`p = wblcdf(x, a, b)`) — @f$ F = 1 - e^{-(x/a)^b} @f$.
Value wblcdf(const Value &x, double a, double b,
             std::pmr::memory_resource *mr = nullptr);

/// Weibull inverse CDF (`x = wblinv(p, a, b)`).
Value wblinv(const Value &p, double a, double b,
             std::pmr::memory_resource *mr = nullptr);

/// Weibull random samples (`r = wblrnd(a, b, rows, cols)`).
Value wblrnd(double a, double b, size_t rows = 1, size_t cols = 1,
             std::pmr::memory_resource *mr = nullptr);

/// Weibull mean / variance (`[m, v] = wblstat(a, b)`).
///
/// `m = a·Γ(1 + 1/b)`, `v = a²·[Γ(1 + 2/b) − Γ(1 + 1/b)²]`.
std::tuple<double, double> wblstat(double a, double b);

} // namespace numkit::stats
