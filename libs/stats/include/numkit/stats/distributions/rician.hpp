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

/// Rician (Rice) density (`y = ricepdf(x, s, sigma)`).
///
/// @f$ f(x; s, \sigma) = (x/\sigma^2)\,e^{-(x^2+s^2)/(2\sigma^2)}\,I_0(xs/\sigma^2) @f$
/// for x ≥ 0, `s ≥ 0`, `sigma > 0`.
Value ricepdf(const Value &x, double s, double sigma,
              std::pmr::memory_resource *mr = nullptr);

/// Rice CDF (`p = ricecdf(x, s, sigma)`) via the Marcum Q-function
/// @f$ F = 1 - Q_1(s/\sigma, x/\sigma) @f$.
Value ricecdf(const Value &x, double s, double sigma,
              std::pmr::memory_resource *mr = nullptr);

/// Rice inverse CDF (`x = riceinv(p, s, sigma)`).
Value riceinv(const Value &p, double s, double sigma,
              std::pmr::memory_resource *mr = nullptr);

/// Rice random samples (`r = ricernd(s, sigma, rows, cols)`).
///
/// Generated as `sqrt(N1² + N2²)` with `N1 ~ N(s, σ²)`, `N2 ~ N(0, σ²)`.
Value ricernd(double s, double sigma, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// Rice mean / variance (`[m, v] = ricestat(s, sigma)`).
std::tuple<double, double>
ricestat(double s, double sigma,
         std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
