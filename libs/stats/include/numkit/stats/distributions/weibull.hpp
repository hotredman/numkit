// libs/stats/include/numkit/stats/distributions/weibull.hpp
//
// Weibull distribution. Convention: a = scale, b = shape, so
// f(x) = (b/a)·(x/a)^(b-1)·exp(-(x/a)^b). Note std::weibull_distribution
// takes (shape, scale) — opposite order.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Weibull pdf (`y = wblpdf(x, a, b)`).
///
/// @f$ f(x; a, b) = (b/a)\,(x/a)^{b-1}\,e^{-(x/a)^b} @f$ for `x >= 0`.
/// Convention: `a` = scale, `b` = shape. Note: `std::weibull_distribution`
/// uses the opposite order `(shape, scale)`.
///
/// @param x   Evaluation points (any shape).
/// @param a   Scale parameter (`a > 0`).
/// @param b   Shape parameter (`b > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pdf values, same shape as `x`.
/// @see wblcdf, wblinv, wblrnd, wblstat
Value wblpdf(const Value &x, double a, double b,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Weibull cdf (`p = wblcdf(x, a, b)`).
///
/// Closed form: @f$ F(x) = 1 - e^{-(x/a)^b} @f$ for `x >= 0`.
///
/// @param x   Evaluation points (any shape).
/// @param a   Scale parameter (`a > 0`).
/// @param b   Shape parameter (`b > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`.
/// @see wblpdf, wblinv
Value wblcdf(const Value &x, double a, double b,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Weibull inverse cdf (`x = wblinv(p, a, b)`).
///
/// Closed form: `x = a · (-log(1 - p))^(1/b)`.
///
/// @param p   Probability levels in `[0, 1]` (any shape).
/// @param a   Scale parameter.
/// @param b   Shape parameter.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `p`.
/// @see wblcdf
Value wblinv(const Value &p, double a, double b,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Weibull random samples (`r = wblrnd(a, b, rows, cols)`).
///
/// @param a     Scale parameter (`a > 0`).
/// @param b     Shape parameter (`b > 0`).
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of Weibull samples.
/// @see wblpdf
Value wblrnd(double a, double b, size_t rows = 1, size_t cols = 1,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Weibull mean and variance (`[m, v] = wblstat(a, b)`).
///
/// Closed form:
/// `m = a · Γ(1 + 1/b)`,
/// `v = a² · [Γ(1 + 2/b) - Γ(1 + 1/b)²]`.
///
/// @param a  Scale parameter.
/// @param b  Shape parameter.
/// @return   `{mean, variance}` pair.
/// @see wblpdf
std::tuple<double, double> wblstat(double a, double b);

/// @brief Weibull MLE fit (`[ahat, bhat] = wblfit(x)`).
///
/// Solves the MLE system for `Weibull(a, b)` (`a` = scale, `b` =
/// shape). The shape `b` satisfies the implicit equation
///
///   `1/b + mean(log x) - Σ x_i^b · log(x_i) / Σ x_i^b = 0`
///
/// solved via Newton iteration on `b` starting from the moment-based
/// initial guess. Scale follows as `a = (Σ x_i^b / n)^{1/b}`.
///
/// @param x   Positive sample data.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `[ahat, bhat]` as a `1 × 2` row (scale, shape).
Value wblfit(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Weibull MLE fit with right-censoring and frequency weights
/// (`[parmhat, parmci] = wblfit(x, alpha, cens, freq)`).
///
/// Likelihood is `∏ f(x_i)^(u_i · f_i) · S(x_i)^(c_i · f_i)` where
/// `S(x; a, b) = exp(-(x/a)^b)` is the Weibull survival. Closed-form
/// fast path used when both `censoring` and `freq` are empty/trivial;
/// otherwise profiles `a(b) = (Σ f_i x_i^b / Σ f_i u_i)^{1/b}` and
/// 1-D Newtons on `b`.
Value wblfit(const Value &x, const Value &censoring, const Value &freq,
             std::pmr::memory_resource *mr = nullptr);

/// @brief 95% Wald CI for wblfit (`pci = wblfit_ci(x, alpha)`).
///
/// Returns the 2 × 2 confidence matrix `[lo; hi]` for `[a, b]` from
/// the observed Fisher information (central-FD Hessian of NLL).
/// Both parameters use a log-scale Wald CI (MATLAB convention).
Value wblfit_ci(const Value &x, double alpha = 0.05,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Wald CI for censored/weighted wblfit
/// (`pci = wblfit_ci(x, alpha, cens, freq)`).
Value wblfit_ci(const Value &x, double alpha,
                const Value &censoring, const Value &freq,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
