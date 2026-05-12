// libs/stats/include/numkit/stats/distributions/hypergeom.hpp
//
// Hypergeometric distribution. MATLAB convention: parameters (M, K, N) where
//   M = population size, K = #successes in population, N = sample size.
// f(k; M, K, N) = C(K, k) · C(M-K, N-k) / C(M, N).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// Hypergeometric pmf (`y = hygepdf(k, M, K, N)`).
///
/// @f$ f(k; M, K, N) = \binom{K}{k}\binom{M-K}{N-k} / \binom{M}{N} @f$.
///
/// @param M  Population size.
/// @param K  Number of successes in population.
/// @param N  Sample size.
Value hygepdf(const Value &k, double M, double K, double N,
              std::pmr::memory_resource *mr = nullptr);

/// Hypergeometric CDF (`F = hygecdf(k, M, K, N)`).
Value hygecdf(const Value &k, double M, double K, double N,
              std::pmr::memory_resource *mr = nullptr);

/// Hypergeometric inverse CDF (`k = hygeinv(q, M, K, N)`).
Value hygeinv(const Value &q, double M, double K, double N,
              std::pmr::memory_resource *mr = nullptr);

/// Hypergeometric random samples (`r = hygernd(M, K, N, rows, cols)`).
Value hygernd(double M, double K, double N,
              size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// Hypergeometric mean / variance (`[m, v] = hygestat(M, K, N)`).
///
/// `m = N·K/M`, `v = N·K(M−K)(M−N) / (M² (M−1))`.
std::tuple<double, double> hygestat(double M, double K, double N);

} // namespace numkit::stats
