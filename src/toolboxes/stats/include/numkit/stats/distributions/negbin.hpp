// toolboxes/stats/include/numkit/stats/distributions/negbin.hpp
//
// Negative binomial (number of failures before the r-th success).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit { namespace ops { class RngContext; } }

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
Value nbinrnd(::numkit::ops::RngContext &rng, double r, double p, size_t rows = 1, size_t cols = 1,
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

/// @brief Negative-binomial MLE fit (`[rhat, phat] = nbinfit(x)`).
///
/// Newton iteration on the profile log-likelihood for the count
/// parameter `r`, with closed-form update for `p = r/(r + mean(x))`
/// at each step. Initial guess from moment matching.
///
/// `x` must be non-negative integers (treated as counts).
///
/// @param x   Non-negative count observations.
/// @param mr  Memory resource.
/// @return    `[rhat, phat]` as a `1 × 2` row.
Value nbinfit(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief 95% Wald CI for nbinfit (`pci = nbinfit_ci(x, alpha)`).
///
/// Returns the 2 × 2 confidence matrix `[lo; hi]` for `[r, p]` from
/// the observed Fisher information at the MLE. `r` uses a log-scale
/// Wald CI; `p` uses a logit-scale Wald CI (MATLAB convention).
Value nbinfit_ci(const Value &x, double alpha = 0.05,
                 std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
