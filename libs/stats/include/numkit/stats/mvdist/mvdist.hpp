// libs/stats/include/numkit/stats/mvdist/mvdist.hpp
//
// Multivariate distribution PDFs / PMFs.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::stats {

/// @brief Multivariate normal pdf (`y = mvnpdf(X, mu, Sigma)`).
///
/// @f$ f(x; \mu, \Sigma) = \dfrac{1}{(2\pi)^{d/2}\,|\Sigma|^{1/2}}\,
///     e^{-\tfrac{1}{2}(x-\mu)^\top \Sigma^{-1} (x-\mu)} @f$.
///
/// @param X      `N × d` (one point per row) or `1 × d` single point.
/// @param mu     `1 × d` mean. Pass empty Value for zeros.
/// @param Sigma  `d × d` covariance (PSD). Pass empty Value for identity.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `N × 1` column of densities.
/// @see mvtpdf, mnpdf
Value mvnpdf(const Value &X, const Value &mu, const Value &Sigma,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Multivariate Student-t pdf (`y = mvtpdf(X, C, df)`).
///
/// `C` is treated as a correlation matrix; if its diagonal is not all
/// 1, the input is normalised to a correlation matrix.
///
/// @param X   `N × d` points.
/// @param C   `d × d` correlation (or covariance — auto-normalised).
/// @param df  Degrees of freedom (`df > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `N × 1` column of densities.
/// @see mvnpdf
Value mvtpdf(const Value &X, const Value &C, double df,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Multinomial pmf (`y = mnpdf(X, P)`).
///
/// @param X   `1 × k` count vector, or `N × k` batch.
/// @param P   `1 × k` probability vector (must sum to 1, entries
///            non-negative).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `N × 1` column of probabilities.
/// @see mvnpdf
Value mnpdf(const Value &X, const Value &P,
            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
