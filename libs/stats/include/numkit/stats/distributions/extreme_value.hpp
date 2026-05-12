// libs/stats/include/numkit/stats/distributions/extreme_value.hpp
//
// Type-I extreme value (Gumbel for minima) distribution. MATLAB
// parameterisation:
//   pdf : (1/σ)·exp(t)·exp(−exp(t)),  t = (x−μ)/σ
//   cdf : 1 − exp(−exp(t))
//   mean: μ − σ·γ_E,  var: σ²·π²/6
// (γ_E is the Euler–Mascheroni constant, ≈ 0.57721566490153286).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// Extreme-value (Type-I, Gumbel-for-minima) density (`y = evpdf(x, mu, sigma)`).
///
/// @f$ f(x; \mu, \sigma) = \frac{1}{\sigma}\,e^{t}\,e^{-e^{t}},\ t = (x-\mu)/\sigma @f$.
Value evpdf(const Value &x, double mu, double sigma,
            std::pmr::memory_resource *mr = nullptr);

/// EV CDF (`p = evcdf(x, mu, sigma)`): @f$ F = 1 - e^{-e^{t}} @f$.
Value evcdf(const Value &x, double mu, double sigma,
            std::pmr::memory_resource *mr = nullptr);

/// EV inverse CDF (`x = evinv(p, mu, sigma)`).
Value evinv(const Value &p, double mu, double sigma,
            std::pmr::memory_resource *mr = nullptr);

/// EV random samples (`r = evrnd(mu, sigma, rows, cols)`).
Value evrnd(double mu, double sigma, size_t rows = 1, size_t cols = 1,
            std::pmr::memory_resource *mr = nullptr);

/// EV mean and variance (`[m, v] = evstat(mu, sigma)`).
///
/// `m = mu - sigma·γ_E`, `v = sigma²·π²/6`.
std::tuple<double, double> evstat(double mu, double sigma);

} // namespace numkit::stats
