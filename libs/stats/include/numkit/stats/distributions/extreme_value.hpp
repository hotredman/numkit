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

Value evpdf(const Value &x, double mu, double sigma, std::pmr::memory_resource *mr = nullptr);
Value evcdf(const Value &x, double mu, double sigma, std::pmr::memory_resource *mr = nullptr);
Value evinv(const Value &p, double mu, double sigma, std::pmr::memory_resource *mr = nullptr);
Value evrnd(double mu, double sigma, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);
std::tuple<double, double> evstat(double mu, double sigma);

} // namespace numkit::stats
