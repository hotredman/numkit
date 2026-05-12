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

/// Beta density (`y = betapdf(x, a, b)`).
///
/// @f$ f(x; a, b) = \frac{x^{a-1}(1-x)^{b-1}}{B(a, b)} @f$ for x ∈ [0, 1].
///
/// @param x   Evaluation point(s).
/// @param a   Shape parameter α > 0.
/// @param b   Shape parameter β > 0.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Density values, same shape as `x`.
Value betapdf(const Value &x, double a, double b,
              std::pmr::memory_resource *mr = nullptr);

/// Beta CDF (`p = betacdf(x, a, b)`).
///
/// @f$ F(x; a, b) = I_x(a, b) @f$ — regularised incomplete beta.
Value betacdf(const Value &x, double a, double b,
              std::pmr::memory_resource *mr = nullptr);

/// Beta inverse CDF (`x = betainv(p, a, b)`) via `betaincinv`.
Value betainv(const Value &p, double a, double b,
              std::pmr::memory_resource *mr = nullptr);

/// Beta random samples (`r = betarnd(a, b, rows, cols)`).
///
/// Generated as `X = U / (U + V)` with `U ~ Gamma(a, 1)`,
/// `V ~ Gamma(b, 1)`.
Value betarnd(double a, double b, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// Beta mean and variance (`[m, v] = betastat(a, b)`).
///
/// `m = a/(a+b)`, `v = a·b / ((a+b)² (a+b+1))`.
std::tuple<double, double> betastat(double a, double b);

} // namespace numkit::stats
