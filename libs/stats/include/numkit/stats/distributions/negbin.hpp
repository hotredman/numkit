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

Value nbinpdf(std::pmr::memory_resource *mr, const Value &k, double r, double p);
Value nbincdf(std::pmr::memory_resource *mr, const Value &k, double r, double p);
Value nbininv(std::pmr::memory_resource *mr, const Value &q, double r, double p);
Value nbinrnd(std::pmr::memory_resource *mr, double r, double p,
              size_t rows = 1, size_t cols = 1);
std::tuple<double, double> nbinstat(double r, double p);

} // namespace numkit::stats
