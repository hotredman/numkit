/// @file fisher_f.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/distributions/fisher_f.hpp
//
// Fisher's F-distribution.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit { namespace ops { class RngContext; } }

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
Value frnd(::numkit::ops::RngContext &rng, double v1, double v2, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

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

/// @brief Noncentral F pdf (`y = ncfpdf(x, nu1, nu2, delta)`).
///
/// Poisson-mixture representation:
///   f(x; ν₁, ν₂, δ) = e^{-δ/2} Σ_{k=0}^∞ (1/k!) (δ/2)^k
///                    · (ν₁/ν₂)^{ν₁/2 + k} · x^{ν₁/2 + k − 1}
///                    · (1 + ν₁ x / ν₂)^{−(ν₁+ν₂)/2 − k}
///                    / B(ν₁/2 + k, ν₂/2)
/// for `x > 0`; series truncated at `1e-16` relative contribution.
/// Reduces to `fpdf(x, ν₁, ν₂)` at `δ = 0`.
///
/// @param x      Evaluation points (any shape).
/// @param nu1    Numerator degrees of freedom (`> 0`).
/// @param nu2    Denominator degrees of freedom (`> 0`).
/// @param delta  Noncentrality parameter (`>= 0`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of pdf values, same shape as `x`.
/// @see ncfcdf, ncfinv, ncfrnd, ncfstat
Value ncfpdf(const Value &x, double nu1, double nu2, double delta,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Noncentral F cdf (`p = ncfcdf(x, nu1, nu2, delta)`).
///
/// Poisson-mixture in the regularised incomplete beta:
///   F(x; ν₁, ν₂, δ) = Σ_k Poisson(k; δ/2) · I_y(ν₁/2 + k, ν₂/2)
/// with `y = ν₁ x / (ν₁ x + ν₂)`. Series truncated at `1e-16`
/// relative contribution. Reduces to `fcdf(x, ν₁, ν₂)` at `δ = 0`.
/// The `'upper'` flag returns `1 - F`.
///
/// @param x      Evaluation points (any shape).
/// @param nu1    Numerator degrees of freedom (`> 0`).
/// @param nu2    Denominator degrees of freedom (`> 0`).
/// @param delta  Noncentrality parameter (`>= 0`).
/// @param upper  If true, return upper-tail `1 - F` (default false).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of cdf values in `[0, 1]`.
/// @see ncfpdf, ncfinv
Value ncfcdf(const Value &x, double nu1, double nu2, double delta,
             bool upper = false, std::pmr::memory_resource *mr = nullptr);

/// @brief Noncentral F inverse cdf (`x = ncfinv(p, nu1, nu2, delta)`).
///
/// Returns `x` such that `ncfcdf(x, ν₁, ν₂, δ) = p`. Newton iteration
/// on `ncfcdf` using `ncfpdf` as derivative; safeguarded by bracketing
/// bisection if Newton steps oscillate. Initial guess from `finv(p, ν₁, ν₂)`.
///
/// @param p      Probability levels in `[0, 1]` (any shape).
/// @param nu1    Numerator degrees of freedom.
/// @param nu2    Denominator degrees of freedom.
/// @param delta  Noncentrality parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Quantile array, same shape as `p`.
/// @see ncfcdf
Value ncfinv(const Value &p, double nu1, double nu2, double delta,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Noncentral F mean and variance
/// (`[m, v] = ncfstat(nu1, nu2, delta)`).
///
/// Closed-form moments:
///   `m = ν₂(ν₁ + δ) / (ν₁(ν₂ - 2))`                         for `ν₂ > 2`,
///   `v = 2(ν₂/ν₁)² ((ν₁+δ)² + (ν₁+2δ)(ν₂-2)) / ((ν₂-2)²(ν₂-4))`
///                                                            for `ν₂ > 4`.
///
/// @param nu1    Numerator degrees of freedom.
/// @param nu2    Denominator degrees of freedom.
/// @param delta  Noncentrality parameter.
/// @return       `{mean, variance}` (NaN where undefined).
/// @see ncfpdf
std::tuple<double, double> ncfstat(double nu1, double nu2, double delta);

/// @brief Noncentral F random samples
/// (`R = ncfrnd(nu1, nu2, delta, rows, cols)`).
///
/// Sampled as `F = (X₁/ν₁) / (X₂/ν₂)` where
/// `X₁ ~ χ²(ν₁, δ)` (noncentral chi-square via Poisson-mixture
/// representation, `J ~ Poisson(δ/2)` then `χ²(ν₁ + 2J)`)
/// and `X₂ ~ χ²(ν₂)`. Uses the shared MT19937 stream.
///
/// @param nu1    Numerator degrees of freedom.
/// @param nu2    Denominator degrees of freedom.
/// @param delta  Noncentrality parameter (`>= 0`).
/// @param rows   Output rows (default 1).
/// @param cols   Output columns (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `rows × cols` matrix of noncentral F samples.
/// @see ncfpdf
Value ncfrnd(::numkit::ops::RngContext &rng, double nu1, double nu2, double delta,
             std::size_t rows = 1, std::size_t cols = 1,
             std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
