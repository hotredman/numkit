// libs/stats/include/numkit/stats/distributions/beta.hpp
//
// Beta distribution. pdf/cdf are direct expressions of the regularized
// incomplete beta; rnd uses the standard X = U/(U+V) construction with
// U ~ Gamma(a, 1), V ~ Gamma(b, 1).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// betapdf(x, a, b) — pdf at x with shape a, b > 0.
Value betapdf(std::pmr::memory_resource *mr, const Value &x, double a, double b);

/// betacdf(x, a, b) — cdf via regularized incomplete beta:
///   F(x) = I_x(a, b)
Value betacdf(std::pmr::memory_resource *mr, const Value &x, double a, double b);

/// betainv(p, a, b) — inverse cdf via betaincinv.
Value betainv(std::pmr::memory_resource *mr, const Value &p, double a, double b);

/// betarnd(a, b[, m, n]) — X = U/(U+V), U~Gamma(a, 1), V~Gamma(b, 1).
Value betarnd(std::pmr::memory_resource *mr, double a, double b,
              size_t rows = 1, size_t cols = 1);

/// betastat(a, b) — mean = a/(a+b), var = ab/((a+b)²(a+b+1)).
std::tuple<double, double> betastat(double a, double b);

} // namespace numkit::stats
