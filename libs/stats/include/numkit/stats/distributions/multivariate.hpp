// libs/stats/include/numkit/stats/distributions/multivariate.hpp
//
// Multivariate distribution primitives.

#pragma once

#include <cstddef>
#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::stats {

/// @brief Multivariate normal random samples
/// (`R = mvnrnd(mu, Sigma [, n])`).
///
/// Draws `n` independent samples from `N(mu, Sigma)` via Cholesky:
/// `R = mu + Z · L'` where `L = chol(Sigma, 'lower')` and `Z` is a
/// standard-normal `n × d` matrix.
///
/// Supports three calling conventions for `mu`:
///   - `1 × d` row vector — same mu for every sample
///   - `d × 1` column vector — same mu for every sample
///   - `n × d` matrix — per-sample location (`n` must match)
///
/// Sigma must be `d × d`, symmetric positive-definite. Non-PD inputs
/// throw `m:mvnrnd:notPD` at the Cholesky step. Sigma-as-diagonal-vector
/// shorthand (`mvnrnd(mu, sigmaVec, n)`) is a v1 KNOWN GAP.
///
/// Uses the shared MT19937 stream so `rng(seed)` makes draws reproducible.
///
/// @param mu     Location parameter (`1×d`, `d×1`, or `n×d`).
/// @param Sigma  Covariance matrix (`d×d`, symmetric PD).
/// @param n      Sample count. `0` → infer from `mu` (1 if `mu` is a vector,
///               `rows(mu)` if `mu` is a matrix).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `n × d` matrix of samples.
/// @throws Error on shape mismatch or non-PD Sigma.
Value mvnrnd(const Value &mu, const Value &Sigma, std::size_t n = 0,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Multivariate normal CDF (`p = mvncdf(X, mu, Sigma)`).
///
/// Returns the probability `P(Y ≤ X)` for `Y ~ N(mu, Sigma)`,
/// row-wise (each row of `X` is one evaluation point).
///
/// Algorithm by dimension:
///   - `d = 1` → direct `normcdf`.
///   - `d = 2` → Owen's tetrachoric / Drezner-Wesolowsky numerical
///               integration (16-point Gauss-Legendre).
///   - `d ≥ 3` → Monte Carlo with antithetic sampling (10000 draws by
///               default). KNOWN GAP: Genz separation-of-variables
///               quasi-MC is more accurate but not yet in v1.
///
/// Mu may be omitted (defaults to zero vector). Sigma defaults to
/// identity. The standard-normal one-arg form `mvncdf(X)` is the
/// most common usage in practice.
///
/// @param X      `n × d` evaluation points (each row one query).
/// @param mu     `1 × d` mean (may be empty → zero).
/// @param Sigma  `d × d` covariance (may be empty → identity).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `n × 1` column of cumulative probabilities.
Value mvncdf(const Value &X, const Value &mu, const Value &Sigma,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Multivariate Student's-t random samples
/// (`R = mvtrnd(C, df, n)`).
///
/// Draws n samples from `t_df(0, C)` via the canonical algorithm:
/// `T = X / sqrt(χ² / df)` where `X ~ N(0, C)` and `χ² ~ χ²(df)`.
///
/// `C` is a `d × d` **correlation** matrix (diagonal = 1). df is the
/// degrees of freedom (positive scalar).
///
/// KNOWN GAP: location parameter not supported (MATLAB also defaults
/// to zero); pass `mu + mvtrnd(...)` at the call site if needed.
///
/// @param C    `d × d` correlation matrix.
/// @param df   Degrees of freedom (`df > 0`).
/// @param n    Sample count.
/// @param mr   Memory resource (nullptr → process default).
/// @return     `n × d` matrix of samples.
Value mvtrnd(const Value &C, double df, std::size_t n,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Multinomial random samples (`R = mnrnd(N, P, m)`).
///
/// Draws m samples from `Multinomial(N, P)` (k categories with
/// probabilities P, total trials N per sample). Returns an `m × k`
/// matrix where each row sums to N.
///
/// `P` must be a length-`k` probability vector (non-negative, sums
/// to 1; renormalised if not). Implementation: per-trial categorical
/// sampling via CDF lookup — O(m · N · k).
///
/// KNOWN GAP: `P` matrix form (per-sample probability rows) not yet
/// supported in v1.
///
/// @param N   Trials per sample (positive integer).
/// @param P   Length-k probability vector.
/// @param m   Number of samples (default 1).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `m × k` count matrix.
Value mnrnd(std::size_t N, const Value &P, std::size_t m = 1,
              std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
