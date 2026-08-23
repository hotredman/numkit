/// @file binomial.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/distributions/binomial.hpp
//
// Binomial distribution.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit {

/// @addtogroup group_stats
/// @{
 namespace ops { class RngContext; } }

namespace numkit::stats {

/// @brief Binomial pmf (`y = binopdf(k, n, p)`).
///
/// @f$ f(k; n, p) = \binom{n}{k}\,p^{k}(1-p)^{n-k} @f$ for `k ∈ {0, …, n}`.
/// Returns 0 outside the support; broadcasts elementwise over `k`.
///
/// @param k   Random-variable values where the pmf is evaluated. Any
///            shape; output matches `k`. Non-integer or out-of-range
///            entries return 0.
/// @param n   Number of trials (positive integer-valued double).
/// @param p   Success probability per trial, `0 ≤ p ≤ 1`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pmf values, same shape as `k`.
/// @see binocdf, binoinv, binornd, binostat
Value binopdf(const Value &k, double n, double p,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Binomial cumulative distribution (`F = binocdf(k, n, p)`).
///
/// Computed via the regularised incomplete-beta identity
/// @f$ F(k) = I_{1-p}(n - \lfloor k\rfloor,\ \lfloor k\rfloor + 1) @f$,
/// which is accurate for the full `k`-range without summing the pmf tail.
///
/// @param k   Evaluation points (any shape).
/// @param n   Number of trials.
/// @param p   Success probability per trial.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`, same shape as `k`.
/// @see binopdf, binoinv
Value binocdf(const Value &k, double n, double p,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Binomial inverse cdf (`k = binoinv(p_in, n, p)`).
///
/// Smallest integer `k` such that `binocdf(k, n, p) >= p_in`.
///
/// @param p_in  Probability levels in `[0, 1]` (any shape).
/// @param n     Number of trials.
/// @param p     Success probability per trial.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Quantile array, same shape as `p_in`.
/// @see binocdf, binopdf
Value binoinv(const Value &p_in, double n, double p,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Binomial random samples (`r = binornd(n, p, rows, cols)`).
///
/// @param n     Number of trials.
/// @param p     Success probability per trial.
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of binomial samples.
/// @see binopdf
Value binornd(::numkit::ops::RngContext &rng, double n, double p, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Binomial mean and variance (`[m, v] = binostat(n, p)`).
///
/// Closed form: `m = n·p`, `v = n·p·(1-p)`.
///
/// @param n  Number of trials.
/// @param p  Success probability per trial.
/// @return   `{mean, variance}` pair.
/// @see binopdf
std::tuple<double, double> binostat(double n, double p);


/// @}
} // namespace numkit::stats
