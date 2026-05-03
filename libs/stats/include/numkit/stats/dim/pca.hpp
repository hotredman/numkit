// libs/stats/include/numkit/stats/dim/pca.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// pca(X) — principal component analysis on N×D data.
/// Returns (coeff, score, latent, tsquared, explained, mu).
///   coeff:    D×D PC matrix (columns are eigenvectors of cov(X))
///   score:    N×D projections (X centred · coeff)
///   latent:   D×1 eigenvalues (sorted descending)
///   tsquared: N×1 Hotelling's T² statistic
///   explained:D×1 percent variance per component
///   mu:       1×D column means subtracted
std::tuple<Value, Value, Value, Value, Value, Value>
pca(std::pmr::memory_resource *mr, const Value &X);

/// pcacov(C) — eigendecompose a D×D covariance matrix.
/// Returns (coeff, latent, explained).
std::tuple<Value, Value, Value>
pcacov(std::pmr::memory_resource *mr, const Value &C);

/// pcares(X, ndim) — residuals after retaining `ndim` PCs.
Value pcares(std::pmr::memory_resource *mr, const Value &X, int ndim);

} // namespace numkit::stats
