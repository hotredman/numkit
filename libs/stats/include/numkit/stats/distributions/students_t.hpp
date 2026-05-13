// libs/stats/include/numkit/stats/distributions/students_t.hpp
//
// Student's t-distribution.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Student's t pdf (`y = tpdf(x, nu)`).
///
/// @f$ f(x; \nu) = \dfrac{\Gamma((\nu+1)/2)}{\sqrt{\nu\pi}\,\Gamma(\nu/2)}
///     \left(1 + x^2/\nu\right)^{-(\nu+1)/2} @f$.
/// Symmetric around 0; converges to standard normal as `nu → ∞`.
///
/// @param x   Evaluation points (any shape).
/// @param nu  Degrees of freedom (`nu > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pdf values, same shape as `x`.
/// @see tcdf, tinv, trnd, tstat
Value tpdf(const Value &x, double nu, std::pmr::memory_resource *mr = nullptr);

/// @brief Student's t cdf (`F = tcdf(x, nu)`).
///
/// Computed via the regularised incomplete beta with the symmetry of the t:
/// for `x >= 0`: `F = 1 - ½·I_{ν/(ν+x²)}(ν/2, ½)`;
/// for `x < 0`:  `F = ½·I_{ν/(ν+x²)}(ν/2, ½)`.
///
/// @param x   Evaluation points (any shape).
/// @param nu  Degrees of freedom (`nu > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`.
/// @see tpdf, tinv
Value tcdf(const Value &x, double nu, std::pmr::memory_resource *mr = nullptr);

/// @brief Student's t inverse cdf (`x = tinv(p, nu)`).
///
/// Computed via `betaincinv`.
///
/// @param p   Probability levels in `[0, 1]` (any shape).
/// @param nu  Degrees of freedom (`nu > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `p`.
/// @see tcdf
Value tinv(const Value &p, double nu, std::pmr::memory_resource *mr = nullptr);

/// @brief Student's t random samples (`r = trnd(nu, rows, cols)`).
///
/// Sampled as `Z / sqrt(X / nu)` where `Z ~ N(0,1)`, `X ~ χ²(nu)`.
///
/// @param nu    Degrees of freedom (`nu > 0`).
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of t-samples.
/// @see tpdf
Value trnd(double nu, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Student's t mean and variance (`[m, v] = tstat(nu)`).
///
/// Closed form: `m = 0` for `nu > 1` (else undefined),
/// `v = nu/(nu - 2)` for `nu > 2` (else `NaN` / `Inf`).
///
/// @param nu  Degrees of freedom.
/// @return    `{mean, variance}` pair (returns NaN for variance when `nu <= 2`).
/// @see tpdf
std::tuple<double, double> tstat(double nu);

} // namespace numkit::stats
