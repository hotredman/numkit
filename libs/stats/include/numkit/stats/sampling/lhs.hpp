// libs/stats/include/numkit/stats/sampling/lhs.hpp
//
// Latin Hypercube sampling.

#pragma once

#include <cstddef>
#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::stats {

/// @brief Latin Hypercube design (`X = lhsdesign(n, p)`).
///
/// Generates an `n × p` matrix where each column is a Latin
/// hypercube sample: the unit interval `(0, 1)` is partitioned into
/// `n` equal bins and exactly one sample lands in each bin. For
/// column `j`, draw a random permutation `π_j` of `1..n` and set
/// `X[i, j] = (π_j[i] − U) / n`, with `U ~ U(0, 1)`. Columns are
/// independent.
///
/// Uses the shared MT19937 stream so `rng(seed)` makes designs
/// reproducible.
///
/// KNOWN GAPs: `'smooth' = 'off'` (midpoint sampling), `'criterion'`
/// (maximin / correlation), and `'iterations'` are deferred.
///
/// @param n   Number of samples (rows).
/// @param p   Number of variables (columns).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `n × p` design matrix with entries in `(0, 1)`.
/// @see lhsnorm
Value lhsdesign(std::size_t n, std::size_t p,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Latin Hypercube sample mapped to a Gaussian
/// (`Y = lhsnorm(mu, Sigma, n)`).
///
/// Apply `norminv` to a `lhsdesign(n, length(mu))` uniform design,
/// then transform via the Cholesky factor of `Sigma`:
/// `Y = Z · chol(Sigma) + mu`, where `Z` is the matrix of standard
/// normal LHS draws.
///
/// `mu` must be a length-`d` row or column vector; `Sigma` must be
/// `d × d` symmetric positive-definite.
///
/// @param mu     Length-`d` mean vector.
/// @param Sigma  `d × d` covariance matrix.
/// @param n      Number of samples.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `n × d` matrix of Gaussian-mapped LHS samples.
/// @see lhsdesign, mvnrnd
Value lhsnorm(const Value &mu, const Value &Sigma, std::size_t n,
              std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
