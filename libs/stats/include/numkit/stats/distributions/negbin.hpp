// libs/stats/include/numkit/stats/distributions/negbin.hpp
//
// Negative binomial (number of failures before the r-th success).
// MATLAB convention: f(k; r, p) = C(k+r-1, k) p^r (1-p)^k, k = 0, 1, 2, ...
// r > 0 (real-valued allowed), 0 < p ≤ 1.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// Negative binomial pmf (`y = nbinpdf(k, r, p)`).
///
/// @f$ f(k; r, p) = \binom{k+r-1}{k}\,p^{r}(1-p)^{k} @f$ for k ≥ 0,
/// `r > 0` (real-valued allowed), `0 < p ≤ 1`.
Value nbinpdf(const Value &k, double r, double p,
              std::pmr::memory_resource *mr = nullptr);

/// Negative binomial CDF (`F = nbincdf(k, r, p)`).
Value nbincdf(const Value &k, double r, double p,
              std::pmr::memory_resource *mr = nullptr);

/// Negative binomial inverse CDF (`k = nbininv(q, r, p)`).
Value nbininv(const Value &q, double r, double p,
              std::pmr::memory_resource *mr = nullptr);

/// Negative binomial random samples (`r_out = nbinrnd(r, p, rows, cols)`).
Value nbinrnd(double r, double p, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// Negative binomial mean / variance (`[m, v] = nbinstat(r, p)`).
///
/// `m = r·(1-p)/p`, `v = r·(1-p)/p²`.
std::tuple<double, double> nbinstat(double r, double p);

} // namespace numkit::stats
