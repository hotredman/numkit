// libs/stats/include/numkit/stats/distributions/fisher_f.hpp
//
// Fisher's F-distribution.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief F pdf (`y = fpdf(x, v1, v2)`).
///
/// @f$ f(x; v_1, v_2) = \dfrac{\Gamma((v_1+v_2)/2)}{\Gamma(v_1/2)\Gamma(v_2/2)}
///     \left(\dfrac{v_1}{v_2}\right)^{v_1/2}
///     x^{v_1/2 - 1}\left(1 + \dfrac{v_1 x}{v_2}\right)^{-(v_1+v_2)/2} @f$
/// for `x > 0`.
///
/// @param x   Evaluation points (any shape).
/// @param v1  Numerator degrees of freedom (`v1 > 0`).
/// @param v2  Denominator degrees of freedom (`v2 > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of pdf values, same shape as `x`.
/// @see fcdf, finv, frnd, fstat
Value fpdf(const Value &x, double v1, double v2, std::pmr::memory_resource *mr = nullptr);

/// @brief F cdf (`F = fcdf(x, v1, v2)`).
///
/// Computed via the regularised incomplete beta:
/// @f$ F(x) = I_{v_1 x / (v_1 x + v_2)}(v_1/2,\ v_2/2) @f$.
///
/// @param x   Evaluation points (any shape).
/// @param v1  Numerator degrees of freedom (`v1 > 0`).
/// @param v2  Denominator degrees of freedom (`v2 > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of cdf values in `[0, 1]`.
/// @see fpdf, finv
Value fcdf(const Value &x, double v1, double v2, std::pmr::memory_resource *mr = nullptr);

/// @brief F inverse cdf (`x = finv(p, v1, v2)`).
///
/// Computed via `betaincinv`.
///
/// @param p   Probability levels in `[0, 1]` (any shape).
/// @param v1  Numerator degrees of freedom.
/// @param v2  Denominator degrees of freedom.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Quantile array, same shape as `p`.
/// @see fcdf
Value finv(const Value &p, double v1, double v2, std::pmr::memory_resource *mr = nullptr);

/// @brief F random samples (`r = frnd(v1, v2, rows, cols)`).
///
/// Sampled as `(X1/v1) / (X2/v2)` where `Xi ~ χ²(vi)`.
///
/// @param v1    Numerator degrees of freedom.
/// @param v2    Denominator degrees of freedom.
/// @param rows  Output rows (default 1).
/// @param cols  Output columns (default 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `rows × cols` matrix of F-samples.
/// @see fpdf
Value frnd(double v1, double v2, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief F mean and variance (`[m, v] = fstat(v1, v2)`).
///
/// Closed form: `m = v2/(v2 - 2)` for `v2 > 2`;
/// variance well-defined only for `v2 > 4` (NaN otherwise).
///
/// @param v1  Numerator degrees of freedom.
/// @param v2  Denominator degrees of freedom.
/// @return    `{mean, variance}` pair.
/// @see fpdf
std::tuple<double, double> fstat(double v1, double v2);

} // namespace numkit::stats
