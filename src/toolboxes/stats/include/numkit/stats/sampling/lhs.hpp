/// @file lhs.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/sampling/lhs.hpp
//
// Latin Hypercube sampling.

#pragma once

#include <cstddef>
#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit {

/// @addtogroup group_stats
/// @{
 namespace ops { class RngContext; } }

namespace numkit::stats {

/// @brief Criterion for `lhsdesign` optimization.
enum class LhsCriterion {
    None,           ///< Single random LHS.
    Maximin,        ///< Maximize the minimum pairwise Euclidean distance.
    Correlation,    ///< Minimize the maximum off-diagonal column correlation.
};

/// @brief Latin Hypercube design (`X = lhsdesign(n, p[, options])`).
///
/// Generates an `n × p` matrix where each column is a Latin
/// hypercube sample: the unit interval `(0, 1)` is partitioned into
/// `n` equal bins and exactly one sample lands in each bin.
///
/// With `smooth = true` (default) the value within each bin is
/// `(π[i] − U)/n` with `U ~ Uniform(0, 1)`; with `smooth = false`
/// midpoints `(π[i] − 0.5)/n` are used.
///
/// `criterion` controls optimization over `iterations` trials:
///   * `None`         – single random design (fastest).
///   * `Maximin`      – pick the trial maximising `min` pairwise
///                      Euclidean distance between rows (DEFAULT,
///                      matches MATLAB).
///   * `Correlation`  – pick the trial minimising `max` absolute
///                      off-diagonal Pearson correlation between
///                      columns.
///
/// Uses the shared MT19937 stream — `rng(seed)` makes designs
/// reproducible. `iterations` default is 5 (MATLAB convention).
///
/// @param n           Number of samples (rows).
/// @param p           Number of variables (columns).
/// @param smooth      `true` for `(π[i] - U)/n`, `false` for midpoints.
/// @param criterion   Optimization objective (see enum).
/// @param iterations  Maximum trials for the criterion search (≥ 1).
/// @param mr          Memory resource (nullptr → process default).
/// @return            `n × p` design matrix with entries in `(0, 1)`.
/// @see lhsnorm
Value lhsdesign(::numkit::ops::RngContext &rng, std::size_t n, std::size_t p,
                bool smooth, LhsCriterion criterion, std::size_t iterations,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Latin Hypercube design with MATLAB defaults
/// (smooth = on, criterion = maximin, iterations = 5).
Value lhsdesign(::numkit::ops::RngContext &rng, std::size_t n, std::size_t p,
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
Value lhsnorm(::numkit::ops::RngContext &rng, const Value &mu, const Value &Sigma, std::size_t n,
              std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::stats
