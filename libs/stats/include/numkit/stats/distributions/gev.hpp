// libs/stats/include/numkit/stats/distributions/gev.hpp
//
// Generalized Extreme Value distribution.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief GEV pdf (`y = gevpdf(x, k, sigma, mu)`).
///
/// Three sub-families depending on the shape `k`:
/// - `k > 0` → Fréchet (heavy-tailed, lower bound `mu - sigma/k`)
/// - `k = 0` → Gumbel-for-maxima limit:
///   `f = (1/σ) e^{-z} e^{-e^{-z}}`, `z = (x-mu)/sigma`
/// - `k < 0` → Reverse Weibull (bounded above at `mu - sigma/k`)
///
/// For `k ≠ 0`, the standardised `t = 1 + k·(x-mu)/sigma` must be `> 0`.
///
/// @param x      Evaluation points (any shape).
/// @param k      Shape parameter (any real).
/// @param sigma  Scale parameter (`sigma > 0`).
/// @param mu     Location parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of pdf values, same shape as `x`.
/// @see gevcdf, gevinv, gevrnd, gevstat
Value gevpdf(const Value &x, double k, double sigma, double mu,
             std::pmr::memory_resource *mr = nullptr);

/// @brief GEV cdf (`p = gevcdf(x, k, sigma, mu)`).
///
/// @param x      Evaluation points (any shape).
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param mu     Location parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of cdf values in `[0, 1]`.
/// @see gevpdf, gevinv
Value gevcdf(const Value &x, double k, double sigma, double mu,
             std::pmr::memory_resource *mr = nullptr);

/// @brief GEV inverse cdf (`x = gevinv(p, k, sigma, mu)`).
///
/// @param p      Probability levels in `[0, 1]` (any shape).
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param mu     Location parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Quantile array, same shape as `p`.
/// @see gevcdf
Value gevinv(const Value &p, double k, double sigma, double mu,
             std::pmr::memory_resource *mr = nullptr);

/// @brief GEV random samples (`r = gevrnd(k, sigma, mu, rows, cols)`).
///
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param mu     Location parameter.
/// @param rows   Output rows (default 1).
/// @param cols   Output columns (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `rows × cols` matrix of GEV samples.
/// @see gevpdf
Value gevrnd(double k, double sigma, double mu,
             size_t rows = 1, size_t cols = 1,
             std::pmr::memory_resource *mr = nullptr);

/// @brief GEV mean and variance (`[m, v] = gevstat(k, sigma, mu)`).
///
/// Mean is defined only for `k < 1`; variance only for `k < 1/2`.
/// Returns `NaN` outside those ranges.
///
/// @param k      Shape parameter.
/// @param sigma  Scale parameter.
/// @param mu     Location parameter.
/// @return       `{mean, variance}` pair.
/// @see gevpdf
std::tuple<double, double>
gevstat(double k, double sigma, double mu);

/// @brief GEV MLE fit (`[parmhat, parmci] = gevfit(x, alpha)`).
///
/// Maximum likelihood estimation of the 3-parameter GEV `(k, σ, μ)`:
///
///   * Initial guess from probability-weighted moments
///     (Hosking, Wallis & Wood 1985):
///       `k_pwm = 7.8590·c + 2.9554·c²` where
///       `c = 2β_1/(β_2 - β_0) − ln 2 / ln 3`,
///       `σ_pwm = k·(β_0 − 2β_1) / (Γ(1+k)·(1 − 2^{-k}))`,
///       `μ_pwm = β_0 + σ_pwm·(1 − Γ(1+k))/k`.
///   * 3-D Newton refinement on `(k, σ, μ)` with FD gradient/Hessian
///     on the log-likelihood, backtracking line search with
///     support-constraint guard (`1 + k(x−μ)/σ > 0` for all x).
///
/// At `k = 0` the support constraint collapses to the Gumbel-max
/// limit; the iteration handles it by treating `|k| < 1e−10` via the
/// small-k Taylor expansion of `log(1+kz)/k ≈ z − k z²/2`.
///
/// @param x      Observations.
/// @param mr     Memory resource.
/// @return       `[khat, sigmahat, muhat]` as a `1 × 3` row.
/// @see gevpdf, gevlike
Value gevfit(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief 95% Wald CI for gevfit (`pci = gevfit_ci(x, alpha)`).
///
/// Returns the 2 × 3 confidence matrix `[lo; hi]` for `[k, σ, μ]`
/// from the observed Fisher information (central-FD Hessian of NLL).
/// `k` and `μ` use linear Wald CI; `σ` uses log-scale (MATLAB
/// convention).
Value gevfit_ci(const Value &x, double alpha = 0.05,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
