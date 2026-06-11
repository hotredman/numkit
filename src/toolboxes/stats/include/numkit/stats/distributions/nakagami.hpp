// toolboxes/stats/include/numkit/stats/distributions/nakagami.hpp
//
// Nakagami distribution.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Nakagami pdf (`y = nakapdf(x, mu, omega)`).
///
/// @f$ f(x; \mu, \Omega) = \dfrac{2\mu^{\mu}}{\Gamma(\mu)\,\Omega^{\mu}}\,
///     x^{2\mu - 1}\,e^{-\mu x^2 / \Omega} @f$ for `x >= 0`.
/// `X² ~ Gamma(shape = mu, scale = omega/mu)`.
///
/// @param x      Evaluation points (any shape).
/// @param mu     Shape parameter (`mu >= 0.5`).
/// @param omega  Spread parameter (`omega > 0`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of pdf values, same shape as `x`.
/// @see nakacdf, nakainv, nakarnd, nakastat
Value nakapdf(const Value &x, double mu, double omega,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Nakagami cdf (`p = nakacdf(x, mu, omega)`).
///
/// Computed via the regularised lower-incomplete gamma:
/// `F(x) = P(mu, mu·x²/omega)`.
///
/// @param x      Evaluation points (any shape).
/// @param mu     Shape parameter (`mu >= 0.5`).
/// @param omega  Spread parameter (`omega > 0`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of cdf values in `[0, 1]`.
/// @see nakapdf, nakainv
Value nakacdf(const Value &x, double mu, double omega,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Nakagami inverse cdf (`x = nakainv(p, mu, omega)`).
///
/// @param p      Probability levels in `[0, 1]` (any shape).
/// @param mu     Shape parameter.
/// @param omega  Spread parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Quantile array, same shape as `p`.
/// @see nakacdf
Value nakainv(const Value &p, double mu, double omega,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Nakagami random samples (`r = nakarnd(mu, omega, rows, cols)`).
///
/// Sampled as `sqrt(G)` with `G ~ Gamma(mu, omega/mu)`.
///
/// @param mu     Shape parameter.
/// @param omega  Spread parameter.
/// @param rows   Output rows (default 1).
/// @param cols   Output columns (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `rows × cols` matrix of Nakagami samples.
/// @see nakapdf
Value nakarnd(double mu, double omega, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Nakagami mean and variance (`[m, v] = nakastat(mu, omega)`).
///
/// Closed form (using `c_mu = Γ(mu + 1/2) / Γ(mu)`):
/// `m = sqrt(omega/mu) · c_mu`,
/// `v = omega · (1 - c_mu² / mu)`.
///
/// @param mu     Shape parameter.
/// @param omega  Spread parameter.
/// @return       `{mean, variance}` pair.
/// @see nakapdf
std::tuple<double, double> nakastat(double mu, double omega);

} // namespace numkit::stats
