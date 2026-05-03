// libs/stats/include/numkit/stats/cluster/distance.hpp
//
// Pairwise distance utilities. These primitives feed every clustering
// algorithm in the toolbox (kmeans, kmedoids, linkage, knnsearch, …).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit::stats {

/// pdist(X[, metric, p]) — pairwise distances between rows of X.
/// Returns a 1×M(M-1)/2 row vector (upper-triangular pack).
/// Supported metrics: euclidean (default), squaredeuclidean, cityblock,
/// chebychev, minkowski, cosine, correlation, hamming, jaccard.
Value pdist(std::pmr::memory_resource *mr, const Value &X,
            const std::string &metric, double p);

/// pdist2(X, Y[, metric, p]) — distances between rows of X (M×D) and
/// rows of Y (N×D). Returns M×N.
Value pdist2(std::pmr::memory_resource *mr, const Value &X, const Value &Y,
             const std::string &metric, double p);

/// squareform(d) — convert a pdist row to a symmetric square matrix
/// (or vice versa: square → pdist row).
Value squareform(std::pmr::memory_resource *mr, const Value &d);

/// mahal(Y, X) — Mahalanobis distance² from each row of Y to the mean
/// of X using the sample covariance of X. Returns N×1 column.
Value mahal(std::pmr::memory_resource *mr, const Value &Y, const Value &X);

} // namespace numkit::stats
