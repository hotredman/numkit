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

Value nakapdf(std::pmr::memory_resource *mr, const Value &x, double mu, double omega);
Value nakacdf(std::pmr::memory_resource *mr, const Value &x, double mu, double omega);
Value nakainv(std::pmr::memory_resource *mr, const Value &p, double mu, double omega);
Value nakarnd(std::pmr::memory_resource *mr, double mu, double omega,
              size_t rows = 1, size_t cols = 1);
std::tuple<double, double> nakastat(double mu, double omega);

} // namespace numkit::stats
