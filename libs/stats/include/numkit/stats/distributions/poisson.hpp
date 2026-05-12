// libs/stats/include/numkit/stats/distributions/poisson.hpp
//
// Poisson distribution. f(k; λ) = λ^k · exp(-λ) / k!  for k = 0, 1, 2, ...
// cdf via Q(s, x) = 1 - gammainc(x, s); rnd uses std::poisson_distribution.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value poisspdf(const Value &k, double lambda, std::pmr::memory_resource *mr = nullptr);
Value poisscdf(const Value &k, double lambda, std::pmr::memory_resource *mr = nullptr);
Value poissinv(const Value &p, double lambda, std::pmr::memory_resource *mr = nullptr);
Value poissrnd(double lambda, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);
std::tuple<double, double> poisstat(double lambda);

} // namespace numkit::stats
