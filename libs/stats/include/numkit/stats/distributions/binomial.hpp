// libs/stats/include/numkit/stats/distributions/binomial.hpp
//
// Binomial distribution. f(k; n, p) = C(n, k) p^k (1-p)^(n-k).
// cdf via betainc identity: F(k; n, p) = I_{1-p}(n - ⌊k⌋, ⌊k⌋ + 1).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value binopdf(std::pmr::memory_resource *mr, const Value &k, double n, double p);
Value binocdf(std::pmr::memory_resource *mr, const Value &k, double n, double p);
Value binoinv(std::pmr::memory_resource *mr, const Value &p_in, double n, double p);
Value binornd(std::pmr::memory_resource *mr, double n, double p,
              size_t rows = 1, size_t cols = 1);
std::tuple<double, double> binostat(double n, double p);

} // namespace numkit::stats
