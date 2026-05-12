// libs/stats/include/numkit/stats/dim/pca.hpp
//
// Principal Component Analysis.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// @brief Principal Component Analysis
/// (`[coeff, score, latent, tsquared, explained, mu] = pca(X)`).
///
/// Standard PCA via eigendecomposition of the sample covariance.
///
/// @param X   `N × D` data matrix (observations as rows).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple:
///            - `coeff`     : `D × D` matrix; columns are PC directions
///              (eigenvectors of `cov(X)`).
///            - `score`     : `N × D` projections (`(X - mu) · coeff`).
///            - `latent`    : `D × 1` eigenvalues (descending).
///            - `tsquared`  : `N × 1` Hotelling's T² statistic.
///            - `explained` : `D × 1` percent variance per component.
///            - `mu`        : `1 × D` column means that were subtracted.
/// @see pcacov, pcares
std::tuple<Value, Value, Value, Value, Value, Value>
pca(const Value &X, std::pmr::memory_resource *mr = nullptr);

/// @brief PCA from a precomputed covariance matrix
/// (`[coeff, latent, explained] = pcacov(C)`).
///
/// Eigendecomposes a `D × D` covariance matrix.
///
/// @param C   `D × D` symmetric covariance matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(coeff, latent, explained)`.
/// @see pca
std::tuple<Value, Value, Value>
pcacov(const Value &C, std::pmr::memory_resource *mr = nullptr);

/// @brief PCA residuals (`res = pcares(X, ndim)`).
///
/// Returns the residual of `X` after projecting onto the first `ndim`
/// principal components: `res = X - (X - mu) · coeff(:, 1:ndim) · coeff(:, 1:ndim)'`.
///
/// @param X     `N × D` data matrix.
/// @param ndim  Number of PCs to retain (`1 <= ndim <= D`).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `N × D` residual matrix.
/// @see pca
Value pcares(const Value &X, int ndim,
             std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
