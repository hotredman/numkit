// toolboxes/stats/include/numkit/stats/distributions/chi2.hpp
//
// Chi-squared distribution. χ²(k) is the sum of k independent
// standard normals squared.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Chi-squared pdf (`y = chi2pdf(x, k)`).
///
/// @f$ f(x; k) = \dfrac{x^{k/2-1}\,e^{-x/2}}{2^{k/2}\,\Gamma(k/2)} @f$
/// for `x > 0` (0 elsewhere). Special case of @ref gampdf with shape `k/2`,
/// scale 2.
///
/// @param x   Evaluation points (any shape; broadcasts elementwise).
/// @param k   Degrees of freedom (`k > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pdf values, same shape as `x`.
/// @see chi2cdf, chi2inv, chi2rnd, chi2stat
Value chi2pdf(const Value &x, double k, std::pmr::memory_resource *mr = nullptr);

/// @brief Chi-squared cdf (`F = chi2cdf(x, k)`).
///
/// Computed via the regularised lower-incomplete gamma:
/// @f$ F(x; k) = P(k/2,\ x/2) = \text{gammainc}(x/2,\ k/2) @f$.
///
/// @param x   Evaluation points (any shape).
/// @param k   Degrees of freedom (`k > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`.
/// @see chi2pdf, chi2inv
Value chi2cdf(const Value &x, double k, std::pmr::memory_resource *mr = nullptr);

/// @brief Chi-squared inverse cdf (`x = chi2inv(p, k)`).
///
/// Computed via `gammaincinv`: `x = 2 · P^{-1}(p; k/2)`.
///
/// @param p   Probability levels in `[0, 1]` (any shape).
/// @param k   Degrees of freedom (`k > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `p`.
/// @see chi2cdf
Value chi2inv(const Value &p, double k, std::pmr::memory_resource *mr = nullptr);

/// @brief Chi-squared random samples (`r = chi2rnd(k, rows, cols)`).
///
/// Sampled as `Gamma(k/2, 2)`.
///
/// @param k     Degrees of freedom (`k > 0`).
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of χ² samples.
/// @see chi2pdf
Value chi2rnd(double k, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Chi-squared mean and variance (`[m, v] = chi2stat(k)`).
///
/// Closed form: `m = k`, `v = 2k`.
///
/// @param k  Degrees of freedom.
/// @return   `{mean, variance}` pair.
/// @see chi2pdf
std::tuple<double, double> chi2stat(double k);

} // namespace numkit::stats
