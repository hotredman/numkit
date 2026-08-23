/// @file students_t.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/distributions/students_t.hpp
//
// Student's t-distribution.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit { namespace ops { class RngContext; } }

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
Value trnd(::numkit::ops::RngContext &rng, double nu, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Student's t mean and variance (`[m, v] = tstat(nu)`).
///
/// Closed form: `m = 0` for `nu > 1` (else undefined),
/// `v = nu/(nu - 2)` for `nu > 2` (else `NaN` / `Inf`).
///
/// @param nu  Degrees of freedom.
/// @return    `{mean, variance}` pair (returns NaN for variance when `nu <= 2`).
/// @see tpdf
std::tuple<double, double> tstat(double nu);

/// @brief Noncentral Student's t pdf (`y = nctpdf(x, nu, delta)`).
///
/// Distribution of `T = (Z + δ)/sqrt(V/ν)` where `Z ~ N(0,1)`,
/// `V ~ χ²(ν)`, `Z ⊥ V`. Reduces to `tpdf(x, ν)` when `δ = 0`.
///
/// Implementation: Owen-Lenth series in terms of the regularised
/// incomplete beta:
///   f(x; ν, δ) = e^{-δ²/2} Σ_k a_k(δ) · g_k(x; ν)
/// where the sum is truncated when contributions fall below
/// `1e-16 · running_sum`.
///
/// @param x      Evaluation points (any shape).
/// @param nu     Degrees of freedom (`nu > 0`).
/// @param delta  Noncentrality parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of pdf values, same shape as `x`.
/// @see nctcdf, nctinv, nctrnd, nctstat
Value nctpdf(const Value &x, double nu, double delta,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Noncentral Student's t cdf (`p = nctcdf(x, nu, delta)`).
///
/// Owen series (1965, Lenth 1989):
///   F(x; ν, δ) = Φ(-δ) + ½·Σ_k P_k · I_y(k+½, ν/2)
///                       + (δ/(2√(2π)))·Σ_k Q_k · I_y(k+1, ν/2)
/// where `y = x²/(x²+ν)`, `P_k = e^{-δ²/2}(δ²/2)^k/k!`,
/// `Q_k = e^{-δ²/2}(δ²/2)^k / Γ(k+3/2)`, and the symmetry
/// `F(x; ν, δ) = 1 - F(-x; ν, -δ)` handles negative `x`.
///
/// The optional `'upper'` flag returns `1 - F`.
///
/// @param x      Evaluation points (any shape).
/// @param nu     Degrees of freedom (`nu > 0`).
/// @param delta  Noncentrality parameter.
/// @param upper  If true, return upper-tail `1 - F` (default false).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Array of cdf values in `[0, 1]`.
/// @see nctpdf, nctinv
Value nctcdf(const Value &x, double nu, double delta, bool upper = false,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Noncentral t inverse cdf (`x = nctinv(p, nu, delta)`).
///
/// Returns `x` such that `nctcdf(x, ν, δ) = p`. Newton iteration on
/// `nctcdf` using `nctpdf` as derivative; safeguarded by bisection if
/// Newton steps oscillate. Initial guess from the central
/// `tinv(p, ν)` shifted by `δ`.
///
/// @param p      Probability levels in `[0, 1]` (any shape).
/// @param nu     Degrees of freedom (`nu > 0`).
/// @param delta  Noncentrality parameter.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Quantile array, same shape as `p`.
/// @see nctcdf
Value nctinv(const Value &p, double nu, double delta,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Noncentral t mean and variance (`[m, v] = nctstat(nu, delta)`).
///
/// Closed-form moments:
///   `m = δ · sqrt(ν/2) · Γ((ν-1)/2) / Γ(ν/2)`    for `ν > 1` (else NaN),
///   `v = ν(1 + δ²)/(ν - 2) − m²`                  for `ν > 2` (else NaN).
///
/// @param nu     Degrees of freedom.
/// @param delta  Noncentrality parameter.
/// @return       `{mean, variance}` (NaN where undefined).
/// @see nctpdf
std::tuple<double, double> nctstat(double nu, double delta);

/// @brief Noncentral Student's t random samples
/// (`r = nctrnd(nu, delta, rows, cols)`).
///
/// Sampled as `T = (Z + δ) / sqrt(V / ν)` where `Z ~ N(0, 1)`,
/// `V ~ χ²(ν)`. Uses the shared MT19937 stream so `rng(seed)`
/// makes draws reproducible.
///
/// @param nu     Degrees of freedom (`nu > 0`).
/// @param delta  Noncentrality parameter.
/// @param rows   Output rows (default 1).
/// @param cols   Output columns (default 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `rows × cols` matrix of nct-samples.
/// @see nctpdf
Value nctrnd(::numkit::ops::RngContext &rng, double nu, double delta, std::size_t rows = 1, std::size_t cols = 1,
             std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
