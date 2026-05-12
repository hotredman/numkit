// libs/stats/include/numkit/stats/distributions/poisson.hpp
//
// Poisson distribution. f(k; λ) = λ^k · exp(-λ) / k!  for k = 0, 1, 2, ...
// cdf via Q(s, x) = 1 - gammainc(x, s); rnd uses std::poisson_distribution.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// Poisson pmf (`y = poisspdf(k, lambda)`).
///
/// @f$ f(k; \lambda) = \lambda^{k}\,e^{-\lambda} / k! @f$ for k = 0, 1, 2, ….
Value poisspdf(const Value &k, double lambda,
               std::pmr::memory_resource *mr = nullptr);

/// Poisson CDF (`F = poisscdf(k, lambda)`).
///
/// Computed via the upper regularised gamma:
/// @f$ F(k) = Q(\lfloor k\rfloor+1, \lambda) = 1 - \text{gammainc}(\lambda,\lfloor k\rfloor+1) @f$.
Value poisscdf(const Value &k, double lambda,
               std::pmr::memory_resource *mr = nullptr);

/// Poisson inverse CDF (`k = poissinv(p, lambda)`).
Value poissinv(const Value &p, double lambda,
               std::pmr::memory_resource *mr = nullptr);

/// Poisson random samples (`r = poissrnd(lambda, rows, cols)`).
Value poissrnd(double lambda, size_t rows = 1, size_t cols = 1,
               std::pmr::memory_resource *mr = nullptr);

/// Poisson mean / variance (`[m, v] = poisstat(lambda) = [lambda, lambda]`).
std::tuple<double, double> poisstat(double lambda);

} // namespace numkit::stats
