/// @file ncx2.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/distributions/ncx2.hpp
//
// Noncentral chi-squared distribution.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit { namespace ops { class RngContext; } }

namespace numkit::stats {

/// @brief Noncentral χ² pdf (`y = ncx2pdf(x, k, lambda)`).
///
/// @f$ f(x; k, \lambda) = \tfrac{1}{2}\,e^{-(x + \lambda)/2}
///     \left(\dfrac{x}{\lambda}\right)^{(k-2)/4} I_{(k-2)/2}(\sqrt{\lambda x}) @f$
/// for `x > 0` when `lambda > 0`. For `lambda == 0` reduces to @ref chi2pdf.
///
/// @param x       Evaluation points (any shape).
/// @param k       Degrees of freedom (`k > 0`).
/// @param lambda  Noncentrality parameter (`lambda >= 0`).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Array of pdf values, same shape as `x`.
/// @see ncx2cdf, ncx2inv, ncx2rnd, ncx2stat, chi2pdf
Value ncx2pdf(const Value &x, double k, double lambda,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Noncentral χ² cdf (`p = ncx2cdf(x, k, lambda)`).
///
/// Computed via the Poisson mixture representation:
/// @f$ F(x) = \sum_{j \ge 0} \text{Poisson}(j;\,\lambda/2)\cdot
///           \chi^{2}_{\text{cdf}}(x;\,k + 2j) @f$.
/// Series truncated when the Poisson tail probability falls below tolerance.
///
/// @param x       Evaluation points (any shape).
/// @param k       Degrees of freedom.
/// @param lambda  Noncentrality parameter.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Array of cdf values in `[0, 1]`.
/// @see ncx2pdf, ncx2inv
Value ncx2cdf(const Value &x, double k, double lambda,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Noncentral χ² inverse cdf (`x = ncx2inv(p, k, lambda)`).
///
/// @param p       Probability levels in `[0, 1]` (any shape).
/// @param k       Degrees of freedom.
/// @param lambda  Noncentrality parameter.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Quantile array, same shape as `p`.
/// @see ncx2cdf
Value ncx2inv(const Value &p, double k, double lambda,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Noncentral χ² random samples (`r = ncx2rnd(k, lambda, rows, cols)`).
///
/// Sampled via the mixture representation
/// `X = Σ_{j ~ Poisson(λ/2)} χ²(k + 2j)`.
///
/// @param k       Degrees of freedom.
/// @param lambda  Noncentrality parameter.
/// @param rows    Output rows (default 1).
/// @param cols    Output columns (default 1).
/// @param mr      Memory resource (nullptr → process default).
/// @return        `rows × cols` matrix of noncentral χ² samples.
/// @see ncx2pdf
Value ncx2rnd(::numkit::ops::RngContext &rng, double k, double lambda, size_t rows = 1, size_t cols = 1,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Noncentral χ² mean and variance (`[m, v] = ncx2stat(k, lambda)`).
///
/// Closed form: `m = k + lambda`, `v = 2·k + 4·lambda`.
///
/// @param k       Degrees of freedom.
/// @param lambda  Noncentrality parameter.
/// @return        `{mean, variance}` pair.
/// @see ncx2pdf
std::tuple<double, double> ncx2stat(double k, double lambda);

} // namespace numkit::stats
