// libs/stats/include/numkit/stats/distributions/extreme_value.hpp
//
// Type-I extreme value (Gumbel for minima) distribution.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Type-I EV (Gumbel-for-minima) pdf (`y = evpdf(x, mu, sigma)`).
///
/// @f$ f(x; \mu, \sigma) = \dfrac{1}{\sigma}\,e^{t}\,e^{-e^{t}},
///     \ t = (x - \mu)/\sigma @f$.
/// Gumbel parameterisation for minima, NOT for maxima.
///
/// @param x      Evaluation points (any shape).
/// @param mu     Location parameter.
/// @param sigma  Scale parameter (`sigma > 0`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of pdf values, same shape as `x`.
/// @see evcdf, evinv, evrnd, evstat
Value evpdf(const Value &x, double mu, double sigma,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Type-I EV cdf (`p = evcdf(x, mu, sigma)`).
///
/// Closed form: @f$ F(x) = 1 - e^{-e^{t}},\ t = (x - \mu)/\sigma @f$.
///
/// @param x      Evaluation points (any shape).
/// @param mu     Location parameter.
/// @param sigma  Scale parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of cdf values in `[0, 1]`.
/// @see evpdf, evinv
Value evcdf(const Value &x, double mu, double sigma,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Type-I EV inverse cdf (`x = evinv(p, mu, sigma)`).
///
/// Closed form: `x = mu + sigma · log(-log(1 - p))`.
///
/// @param p      Probability levels in `[0, 1]` (any shape).
/// @param mu     Location parameter.
/// @param sigma  Scale parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Quantile array, same shape as `p`.
/// @see evcdf
Value evinv(const Value &p, double mu, double sigma,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Type-I EV random samples (`r = evrnd(mu, sigma, rows, cols)`).
///
/// Generated via inverse-CDF sampling.
///
/// @param mu     Location parameter.
/// @param sigma  Scale parameter.
/// @param rows   Output rows (default 1).
/// @param cols   Output columns (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `rows × cols` matrix of EV samples.
/// @see evpdf
Value evrnd(double mu, double sigma, size_t rows = 1, size_t cols = 1,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Type-I EV mean and variance (`[m, v] = evstat(mu, sigma)`).
///
/// Closed form: `m = mu - sigma · γ_E`, `v = sigma² · π²/6`
/// (γ_E ≈ 0.57721566490153286 is the Euler-Mascheroni constant).
///
/// @param mu     Location parameter.
/// @param sigma  Scale parameter.
/// @return       `{mean, variance}` pair.
/// @see evpdf
std::tuple<double, double> evstat(double mu, double sigma);

/// @brief Type-I EV (Gumbel-min) MLE fit (`[muhat, sigmahat] = evfit(x)`).
///
/// Concentrates the location out via `μ = σ · log(Σ exp(x_i/σ) / n)` and
/// runs Newton iteration on the resulting 1-D equation
/// `Σ x_i e^{x_i/σ} / Σ e^{x_i/σ} - mean(x) - σ = 0`. Initial guess from
/// the moment relation `var(x) = σ² · π²/6`.
///
/// Numerical stability: exponentials shifted by `max(x)/σ` (the ratio
/// `U/T` is invariant under that shift).
///
/// @param x   Observations.
/// @param mr  Memory resource.
/// @return    `[muhat, sigmahat]` as a `1 × 2` row.
/// @see evpdf, evcdf
Value evfit(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Type-I EV (Gumbel-min) MLE fit with right-censoring and
/// frequency weights (`[parmhat, parmci] = evfit(x, alpha, cens, freq)`).
///
/// Likelihood is the product of `f(x_i)` over uncensored observations
/// and `S(x_i)` (survival) over right-censored observations, each
/// raised to the frequency-weight `freq_i`. When both `censoring` and
/// `freq` are empty or trivial, dispatches to the fast closed-form
/// path; otherwise refines via 2-D Newton on (μ, σ) with FD gradient
/// and Hessian and a backtracking line search.
///
/// @param x          Observations.
/// @param censoring  Length-`n` 0/1 indicator (1 = right-censored).
///                   Pass `Value::Empty` to disable.
/// @param freq       Length-`n` non-negative frequency weights.
///                   Pass `Value::Empty` to disable (uniform weight).
/// @param mr         Memory resource.
/// @return           `[muhat, sigmahat]` as a `1 × 2` row.
Value evfit(const Value &x, const Value &censoring, const Value &freq,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Wald CI for evfit (`pci = evfit_ci(x, alpha)`).
///
/// Returns the 2 × 2 confidence matrix `[lo; hi]` for `[mu, sigma]`
/// from the observed Fisher information (central-FD Hessian of NLL).
/// `mu` uses a linear Wald CI; `sigma` uses a log-scale CI (MATLAB
/// convention).
Value evfit_ci(const Value &x, double alpha = 0.05,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Wald CI for censored/weighted evfit
/// (`pci = evfit_ci(x, alpha, cens, freq)`).
Value evfit_ci(const Value &x, double alpha,
               const Value &censoring, const Value &freq,
               std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
