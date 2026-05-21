// libs/stats/include/numkit/stats/distributions/negbin.hpp
//
// Negative binomial (number of failures before the r-th success).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Negative binomial pmf (`y = nbinpdf(k, r, p)`).
///
/// @f$ f(k; r, p) = \binom{k + r - 1}{k}\,p^{r}(1-p)^{k} @f$ for `k = 0, 1, 2, …`.
/// Convention: `k` counts failures before the `r`-th success;
/// `r > 0` real-valued allowed; `0 < p ≤ 1`.
///
/// @param k   Evaluation points (any shape).
/// @param r   Number of successes (`r > 0`).
/// @param p   Success probability per trial.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pmf values, same shape as `k`.
/// @see nbincdf, nbininv, nbinrnd, nbinstat
Value nbinpdf(const Value &k, double r, double p,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Negative binomial cdf (`F = nbincdf(k, r, p)`).
///
/// Computed via the regularised incomplete beta:
/// @f$ F(k) = I_{p}(r,\ \lfloor k\rfloor + 1) @f$.
///
/// @param k   Evaluation points (any shape).
/// @param r   Number of successes.
/// @param p   Success probability per trial.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`.
/// @see nbinpdf, nbininv
Value nbincdf(const Value &k, double r, double p,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Negative binomial inverse cdf (`k = nbininv(q, r, p)`).
///
/// Smallest non-negative integer `k` such that `nbincdf(k, r, p) >= q`.
///
/// @param q   Probability levels in `[0, 1]` (any shape).
/// @param r   Number of successes.
/// @param p   Success probability per trial.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `q`.
/// @see nbincdf
Value nbininv(const Value &q, double r, double p,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Negative binomial random samples (`r_out = nbinrnd(r, p, rows, cols)`).
///
/// @param r     Number of successes (`r > 0`).
/// @param p     Success probability per trial.
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of negative-binomial samples.
/// @see nbinpdf
Value nbinrnd(double r, double p, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Negative binomial mean and variance (`[m, v] = nbinstat(r, p)`).
///
/// Closed form:
/// `m = r·(1-p)/p`,
/// `v = r·(1-p)/p²`.
///
/// @param r  Number of successes.
/// @param p  Success probability per trial.
/// @return   `{mean, variance}` pair.
/// @see nbinpdf
std::tuple<double, double> nbinstat(double r, double p);

} // namespace numkit::stats
