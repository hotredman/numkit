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

/// Generalised Pareto density (`y = gppdf(x, k, sigma, theta)`).
///
/// Threshold `theta`, scale `sigma > 0`, shape `k`.
Value gppdf(const Value &x, double k, double sigma, double theta,
            std::pmr::memory_resource *mr = nullptr);

/// GP CDF (`p = gpcdf(x, k, sigma, theta)`).
Value gpcdf(const Value &x, double k, double sigma, double theta,
            std::pmr::memory_resource *mr = nullptr);

/// GP inverse CDF (`x = gpinv(p, k, sigma, theta)`).
Value gpinv(const Value &p, double k, double sigma, double theta,
            std::pmr::memory_resource *mr = nullptr);

/// GP random samples (`r = gprnd(k, sigma, theta, rows, cols)`).
Value gprnd(double k, double sigma, double theta,
            size_t rows = 1, size_t cols = 1,
            std::pmr::memory_resource *mr = nullptr);

/// GP mean / variance (`[m, v] = gpstat(k, sigma, theta)`).
///
/// Defined only for `k < 1` (mean) and `k < 1/2` (variance); falls
/// back to NaN outside those ranges.
std::tuple<double, double>
gpstat(double k, double sigma, double theta);

} // namespace numkit::stats
