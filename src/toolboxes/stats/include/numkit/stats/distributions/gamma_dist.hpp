/// @file gamma_dist.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/distributions/gamma_dist.hpp
//
// Gamma distribution Gamma(a, b): a = shape, b = scale.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit {

/// @addtogroup group_stats
/// @{
 namespace ops { class RngContext; } }

namespace numkit::stats {

/// @brief Gamma pdf (`y = gampdf(x, a, b)`).
///
/// @f$ f(x; a, b) = \dfrac{x^{a-1}\,e^{-x/b}}{b^{a}\,\Gamma(a)} @f$ for `x > 0`.
/// Convention: `a` is the shape, `b` is the scale (NOT rate).
///
/// @param x   Evaluation points (any shape).
/// @param a   Shape parameter (`a > 0`).
/// @param b   Scale parameter (`b > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pdf values, same shape as `x`.
/// @see gamcdf, gaminv, gamrnd, gamstat
Value gampdf(const Value &x, double a, double b, std::pmr::memory_resource *mr = nullptr);

/// @brief Gamma cdf (`F = gamcdf(x, a, b)`).
///
/// Computed via the regularised lower-incomplete gamma:
/// @f$ F(x) = P(a,\ x/b) = \text{gammainc}(x/b,\ a) @f$.
///
/// @param x   Evaluation points (any shape).
/// @param a   Shape parameter (`a > 0`).
/// @param b   Scale parameter (`b > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`.
/// @see gampdf, gaminv
Value gamcdf(const Value &x, double a, double b, std::pmr::memory_resource *mr = nullptr);

/// @brief Gamma inverse cdf (`x = gaminv(p, a, b)`).
///
/// Computed via `gammaincinv`: `x = b · P^{-1}(p; a)`.
///
/// @param p   Probability levels in `[0, 1]` (any shape).
/// @param a   Shape parameter (`a > 0`).
/// @param b   Scale parameter (`b > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `p`.
/// @see gamcdf
Value gaminv(const Value &p, double a, double b, std::pmr::memory_resource *mr = nullptr);

/// @brief Gamma random samples (`r = gamrnd(a, b, rows, cols)`).
///
/// Uses `std::gamma_distribution` with shape `a`, scale `b`.
///
/// @param a     Shape parameter (`a > 0`).
/// @param b     Scale parameter (`b > 0`).
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of Gamma samples.
/// @see gampdf
Value gamrnd(::numkit::ops::RngContext &rng, double a, double b, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Gamma mean and variance (`[m, v] = gamstat(a, b)`).
///
/// Closed form: `m = a·b`, `v = a·b²`.
///
/// @param a  Shape parameter.
/// @param b  Scale parameter.
/// @return   `{mean, variance}` pair.
/// @see gampdf
std::tuple<double, double> gamstat(double a, double b);

/// @brief Raw gamma(shape, 1) RNG (`r = randg(shape [, rows, cols])`).
///
/// MATLAB's undocumented-but-widely-used "raw" gamma sampler. Equivalent
/// to `gamrnd(shape, 1.0, rows, cols)`; uses the shared MT19937 stream
/// so `rng(seed)` reproduces the draw sequence.
///
/// @param shape  Shape parameter (`shape > 0`); array-valued shape is
///               supported via the adapter-only entry — see `randg_reg`.
/// @param rows   Output rows (default 1).
/// @param cols   Output columns (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `rows × cols` matrix of Gamma(shape, 1) samples.
/// @see gamrnd
Value randg(::numkit::ops::RngContext &rng, double shape, size_t rows = 1, size_t cols = 1,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Per-element gamma RNG (`r = randg(shapeArray)`).
///
/// Draws one sample per entry of `shapeArray`, with shape = element value.
/// Negative / zero shape entries produce NaN in the output (MATLAB
/// convention).
///
/// @param shapeArray  Array-valued shape parameter.
/// @param mr          Memory resource (nullptr → process default).
/// @return            Array same size as `shapeArray`, one Gamma(shape, 1)
///                    draw per entry.
/// @see randg
Value randg(::numkit::ops::RngContext &rng, const Value &shapeArray,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Gamma distribution MLE fit (`[ahat, bhat] = gamfit(x)`).
///
/// Solves the MLE system for `Gamma(a, b)` on positive data. The
/// shape `a` is found by Newton iteration on `log a - ψ(a) = s` where
/// `s = log(mean(x)) - mean(log(x))` (Minka 2002 initial guess +
/// digamma/trigamma Newton); the scale `b = mean(x) / a`.
///
/// Returns a 2-element vector `[ahat, bhat]`. KNOWN GAP: confidence
/// intervals (`bci` second output) deferred — MATLAB ships them but
/// the v1 numkit form is single-output.
///
/// @param x   Positive sample data (1-D vector or matrix; flattened).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `[ahat, bhat]` as a `1 × 2` row.
Value gamfit(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Gamma MLE fit with right-censoring and frequency weights
/// (`[parmhat, parmci] = gamfit(x, alpha, cens, freq)`).
///
/// Likelihood is `∏ f(x_i)^(u_i · f_i) · S(x_i)^(c_i · f_i)` where the
/// survival is `S(x; a, b) = 1 − gammainc(x/b, a)` (upper-tail
/// regularised incomplete gamma). When `censoring` and `freq` are both
/// empty/trivial, falls through to the fast Newton path. Otherwise
/// runs 2-D Newton on (a, b) with FD gradient/Hessian on the
/// censored-weighted NLL, with backtracking line search.
Value gamfit(const Value &x, const Value &censoring, const Value &freq,
             std::pmr::memory_resource *mr = nullptr);

/// @brief 95% Wald CI for gamfit (`pci = gamfit_ci(x, alpha)`).
///
/// Returns the 2 × 2 confidence matrix `[lo; hi]` for `[a, b]` from
/// the observed Fisher information at the MLE (central-FD Hessian).
/// Both parameters use a log-scale Wald CI (MATLAB convention).
Value gamfit_ci(const Value &x, double alpha = 0.05,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Wald CI for censored/weighted gamfit
/// (`pci = gamfit_ci(x, alpha, cens, freq)`).
Value gamfit_ci(const Value &x, double alpha,
                const Value &censoring, const Value &freq,
                std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::stats
