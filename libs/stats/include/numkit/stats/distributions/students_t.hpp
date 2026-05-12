// libs/stats/include/numkit/stats/distributions/students_t.hpp
//
// Student's t-distribution. Built from incomplete beta + chi-squared
// random sampler.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// tpdf(x, nu) — pdf at x with nu > 0 degrees of freedom.
Value tpdf(const Value &x, double nu, std::pmr::memory_resource *mr = nullptr);

/// tcdf(x, nu) — cdf via the symmetry of the t-distribution and the
/// regularized incomplete beta:
///   For x ≥ 0: F = 1 - ½ · I_{ν/(ν+x²)}(ν/2, ½)
///   For x < 0: F = ½ · I_{ν/(ν+x²)}(ν/2, ½)
Value tcdf(const Value &x, double nu, std::pmr::memory_resource *mr = nullptr);

/// tinv(p, nu) — inverse cdf via betaincinv.
Value tinv(const Value &p, double nu, std::pmr::memory_resource *mr = nullptr);

/// trnd(nu[, m, n]) — Z / sqrt(X / nu) where Z ~ N(0,1), X ~ χ²(ν).
Value trnd(double nu, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// tstat(nu) — [0, nu/(nu-2)] for nu > 2; variance NaN for nu ≤ 2.
std::tuple<double, double> tstat(double nu);

} // namespace numkit::stats
