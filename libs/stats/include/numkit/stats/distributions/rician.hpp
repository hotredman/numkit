// libs/stats/include/numkit/stats/distributions/rician.hpp
//
// Rician (Rice) distribution. Parameters: noncentrality s ≥ 0, scale σ > 0.
//   pdf:  (x/σ²) · exp(−(x²+s²)/(2σ²)) · I_0(x·s/σ²),  x ≥ 0
//   cdf:  1 − Q_1(s/σ, x/σ)  (Marcum Q-function with m = 1)
//   X = √(N1² + N2²) where N1 ~ N(s, σ²), N2 ~ N(0, σ²).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value ricepdf(std::pmr::memory_resource *mr, const Value &x, double s, double sigma);
Value ricecdf(std::pmr::memory_resource *mr, const Value &x, double s, double sigma);
Value riceinv(std::pmr::memory_resource *mr, const Value &p, double s, double sigma);
Value ricernd(std::pmr::memory_resource *mr, double s, double sigma,
              size_t rows = 1, size_t cols = 1);
std::tuple<double, double> ricestat(std::pmr::memory_resource *mr,
                                    double s, double sigma);

} // namespace numkit::stats
