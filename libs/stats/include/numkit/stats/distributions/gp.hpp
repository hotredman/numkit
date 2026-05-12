// libs/stats/include/numkit/stats/distributions/gp.hpp
//
// Generalized Pareto distribution. MATLAB convention:
// gp*(x, k, sigma, theta).
//   k = 0  → exponential with mean σ
//   k > 0  → heavy-tailed (support x ≥ θ)
//   k < 0  → bounded (support x in [θ, θ − σ/k])

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value gppdf(const Value &x, double k, double sigma, double theta, std::pmr::memory_resource *mr = nullptr);
Value gpcdf(const Value &x, double k, double sigma, double theta, std::pmr::memory_resource *mr = nullptr);
Value gpinv(const Value &p, double k, double sigma, double theta, std::pmr::memory_resource *mr = nullptr);
Value gprnd(double k, double sigma, double theta, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);
std::tuple<double, double>
gpstat(double k, double sigma, double theta);

} // namespace numkit::stats
