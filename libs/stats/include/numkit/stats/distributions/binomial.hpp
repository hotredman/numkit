// libs/stats/include/numkit/stats/distributions/binomial.hpp
//
// Binomial distribution. f(k; n, p) = C(n, k) p^k (1-p)^(n-k).
// cdf via betainc identity: F(k; n, p) = I_{1-p}(n - ⌊k⌋, ⌊k⌋ + 1).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// Binomial pmf (`y = binopdf(k, n, p)`).
///
/// @f$ f(k; n, p) = \binom{n}{k}\,p^{k}(1-p)^{n-k} @f$ for k ∈ {0, …, n}.
Value binopdf(const Value &k, double n, double p,
              std::pmr::memory_resource *mr = nullptr);

/// Binomial CDF (`F = binocdf(k, n, p)`) via the regularised incomplete
/// beta identity @f$ F(k) = I_{1-p}(n - \lfloor k\rfloor, \lfloor k\rfloor + 1) @f$.
Value binocdf(const Value &k, double n, double p,
              std::pmr::memory_resource *mr = nullptr);

/// Binomial inverse CDF (`k = binoinv(p_in, n, p)`).
Value binoinv(const Value &p_in, double n, double p,
              std::pmr::memory_resource *mr = nullptr);

/// Binomial random samples (`r = binornd(n, p, rows, cols)`).
Value binornd(double n, double p, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// Binomial mean / variance (`[m, v] = binostat(n, p)`) — `m = np`, `v = np(1-p)`.
std::tuple<double, double> binostat(double n, double p);

} // namespace numkit::stats
