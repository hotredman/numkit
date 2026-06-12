// toolboxes/stats/include/numkit/stats/distributions/hypergeom.hpp
//
// Hypergeometric distribution.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit { namespace ops { class RngContext; } }

namespace numkit::stats {

/// @brief Hypergeometric pmf (`y = hygepdf(k, M, K, N)`).
///
/// @f$ f(k; M, K, N) = \binom{K}{k}\,\binom{M-K}{N-k} \big/ \binom{M}{N} @f$.
///
/// @param k   Evaluation points (any shape).
/// @param M   Population size.
/// @param K   Number of successes in population.
/// @param N   Sample size.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pmf values, same shape as `k`.
/// @see hygecdf, hygeinv, hygernd, hygestat
Value hygepdf(const Value &k, double M, double K, double N,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Hypergeometric cdf (`F = hygecdf(k, M, K, N)`).
///
/// @param k   Evaluation points (any shape).
/// @param M   Population size.
/// @param K   Number of successes in population.
/// @param N   Sample size.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`.
/// @see hygepdf, hygeinv
Value hygecdf(const Value &k, double M, double K, double N,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Hypergeometric inverse cdf (`k = hygeinv(q, M, K, N)`).
///
/// @param q   Probability levels in `[0, 1]` (any shape).
/// @param M   Population size.
/// @param K   Number of successes in population.
/// @param N   Sample size.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `q`.
/// @see hygecdf
Value hygeinv(const Value &q, double M, double K, double N,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Hypergeometric random samples (`r = hygernd(M, K, N, rows, cols)`).
///
/// @param M     Population size.
/// @param K     Number of successes in population.
/// @param N     Sample size.
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of hypergeometric samples.
/// @see hygepdf
Value hygernd(::numkit::ops::RngContext &rng, double M, double K, double N,
              size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Hypergeometric mean and variance (`[m, v] = hygestat(M, K, N)`).
///
/// Closed form:
/// `m = N·K/M`,
/// `v = N·K·(M-K)·(M-N) / (M² · (M-1))`.
///
/// @param M  Population size.
/// @param K  Number of successes in population.
/// @param N  Sample size.
/// @return   `{mean, variance}` pair.
/// @see hygepdf
std::tuple<double, double> hygestat(double M, double K, double N);

} // namespace numkit::stats
