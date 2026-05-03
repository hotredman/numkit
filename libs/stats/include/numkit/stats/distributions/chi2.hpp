// libs/stats/include/numkit/stats/distributions/chi2.hpp
//
// Chi-squared distribution. χ²(k) is the sum of k independent
// standard normals squared.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// chi2pdf(x, k) — pdf at x with k > 0 degrees of freedom.
Value chi2pdf(std::pmr::memory_resource *mr, const Value &x, double k);

/// chi2cdf(x, k) — cdf, computed via the regularized lower-incomplete
/// gamma function: F(x, k) = P(k/2, x/2) = gammainc(x/2, k/2).
Value chi2cdf(std::pmr::memory_resource *mr, const Value &x, double k);

/// chi2inv(p, k) — inverse cdf via gammaincinv.
Value chi2inv(std::pmr::memory_resource *mr, const Value &p, double k);

/// chi2rnd(k[, m, n]) — sample from χ²(k) via Gamma(k/2, 2).
Value chi2rnd(std::pmr::memory_resource *mr, double k,
              size_t rows = 1, size_t cols = 1);

/// chi2stat(k) — [mean, variance] = [k, 2k].
std::tuple<double, double> chi2stat(double k);

} // namespace numkit::stats
