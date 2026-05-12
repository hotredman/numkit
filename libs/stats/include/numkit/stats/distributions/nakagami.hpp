// libs/stats/include/numkit/stats/distributions/nakagami.hpp
//
// Nakagami distribution. Parameters: shape μ ≥ 0.5, spread Ω > 0.
//   pdf:  (2 μ^μ / Γ(μ) Ω^μ) · x^(2μ−1) · exp(−μ x²/Ω),  x ≥ 0
//   cdf:  P(μ, μ x²/Ω)        (regularised lower-gamma)
//   X²  ~ Gamma(μ, Ω/μ) (shape μ, scale Ω/μ)

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value nakapdf(const Value &x, double mu, double omega, std::pmr::memory_resource *mr = nullptr);
Value nakacdf(const Value &x, double mu, double omega, std::pmr::memory_resource *mr = nullptr);
Value nakainv(const Value &p, double mu, double omega, std::pmr::memory_resource *mr = nullptr);
Value nakarnd(double mu, double omega, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);
std::tuple<double, double> nakastat(double mu, double omega);

} // namespace numkit::stats
