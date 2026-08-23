/// @file beta.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/distributions/beta.hpp
//
// Beta distribution.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit { namespace ops { class RngContext; } }

namespace numkit::stats {

/// @brief Beta pdf (`y = betapdf(x, a, b)`).
///
/// @f$ f(x; a, b) = \dfrac{x^{a-1}(1-x)^{b-1}}{B(a, b)} @f$ for `x ∈ [0, 1]`,
/// 0 elsewhere.
///
/// @param x   Evaluation points (any shape).
/// @param a   First shape parameter (`a > 0`).
/// @param b   Second shape parameter (`b > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pdf values, same shape as `x`.
/// @see betacdf, betainv, betarnd, betastat
Value betapdf(const Value &x, double a, double b,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Beta cdf (`p = betacdf(x, a, b)`).
///
/// Direct expression of the regularised incomplete beta:
/// @f$ F(x; a, b) = I_x(a, b) @f$.
///
/// @param x   Evaluation points (any shape).
/// @param a   First shape parameter (`a > 0`).
/// @param b   Second shape parameter (`b > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`.
/// @see betapdf, betainv
Value betacdf(const Value &x, double a, double b,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Beta inverse cdf (`x = betainv(p, a, b)`).
///
/// Computed via `betaincinv`.
///
/// @param p   Probability levels in `[0, 1]` (any shape).
/// @param a   First shape parameter.
/// @param b   Second shape parameter.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `p`.
/// @see betacdf
Value betainv(const Value &p, double a, double b,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Beta random samples (`r = betarnd(a, b, rows, cols)`).
///
/// Generated as `X = U / (U + V)` with `U ~ Gamma(a, 1)`, `V ~ Gamma(b, 1)`.
///
/// @param a     First shape parameter (`a > 0`).
/// @param b     Second shape parameter (`b > 0`).
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of Beta samples.
/// @see betapdf
Value betarnd(::numkit::ops::RngContext &rng, double a, double b, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Beta mean and variance (`[m, v] = betastat(a, b)`).
///
/// Closed form:
/// `m = a / (a + b)`,
/// `v = a·b / ((a + b)² · (a + b + 1))`.
///
/// @param a  First shape parameter.
/// @param b  Second shape parameter.
/// @return   `{mean, variance}` pair.
/// @see betapdf
std::tuple<double, double> betastat(double a, double b);

/// @brief Beta(a, b) MLE fit (`[ahat, bhat] = betafit(x)`).
///
/// Newton iteration on the digamma system
///   `ψ(a) - ψ(a+b) = mean log(x)`
///   `ψ(b) - ψ(a+b) = mean log(1-x)`
/// Initial guess from method of moments. Throws on x outside [0, 1].
///
/// @param x   Samples in [0, 1].
/// @param mr  Memory resource.
/// @return    `[ahat, bhat]` as a `1 × 2` row.
Value betafit(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief 95% Wald CI for betafit (`pci = betafit_ci(x, alpha)`).
///
/// Returns the 2 × 2 confidence matrix `[lo; hi]` for `[a, b]` from
/// the observed Fisher information (central-FD Hessian of NLL).
/// Both parameters use a log-scale Wald CI (MATLAB convention).
Value betafit_ci(const Value &x, double alpha = 0.05,
                 std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
