// libs/stats/include/numkit/stats/distributions/fisher_f.hpp
//
// Fisher's F-distribution. Ratio of two independent χ² variables
// (each scaled by their degrees of freedom).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// fpdf(x, v1, v2) — pdf at x with v1, v2 > 0 degrees of freedom.
Value fpdf(std::pmr::memory_resource *mr, const Value &x, double v1, double v2);

/// fcdf(x, v1, v2) — cdf via regularized incomplete beta:
///   F(x) = I_{v1·x/(v1·x + v2)}(v1/2, v2/2)
Value fcdf(std::pmr::memory_resource *mr, const Value &x, double v1, double v2);

/// finv(p, v1, v2) — inverse cdf via betaincinv.
Value finv(std::pmr::memory_resource *mr, const Value &p, double v1, double v2);

/// frnd(v1, v2[, m, n]) — (X1/v1) / (X2/v2), Xi ~ χ²(vi).
Value frnd(std::pmr::memory_resource *mr, double v1, double v2,
           size_t rows = 1, size_t cols = 1);

/// fstat(v1, v2) — mean = v2/(v2-2) for v2 > 2; variance defined for v2 > 4.
std::tuple<double, double> fstat(double v1, double v2);

} // namespace numkit::stats
