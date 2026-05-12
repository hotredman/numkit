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

/// Nakagami density (`y = nakapdf(x, mu, omega)`).
///
/// @f$ f(x; \mu, \Omega) = \frac{2\mu^{\mu}}{\Gamma(\mu)\,\Omega^{\mu}}\,x^{2\mu-1}\,e^{-\mu x^2/\Omega} @f$
/// for x ≥ 0; `mu ≥ 0.5`, `omega > 0`.
Value nakapdf(const Value &x, double mu, double omega,
              std::pmr::memory_resource *mr = nullptr);

/// Nakagami CDF (`p = nakacdf(x, mu, omega)`).
Value nakacdf(const Value &x, double mu, double omega,
              std::pmr::memory_resource *mr = nullptr);

/// Nakagami inverse CDF (`x = nakainv(p, mu, omega)`).
Value nakainv(const Value &p, double mu, double omega,
              std::pmr::memory_resource *mr = nullptr);

/// Nakagami random samples (`r = nakarnd(mu, omega, rows, cols)`).
Value nakarnd(double mu, double omega, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// Nakagami mean / variance (`[m, v] = nakastat(mu, omega)`).
std::tuple<double, double> nakastat(double mu, double omega);

} // namespace numkit::stats
