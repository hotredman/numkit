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
Value pdist(const Value &X, const std::string &metric, double p, std::pmr::memory_resource *mr = nullptr);

/// pdist2(X, Y[, metric, p]) — distances between rows of X (M×D) and
/// rows of Y (N×D). Returns M×N.
Value pdist2(const Value &X, const Value &Y, const std::string &metric, double p, std::pmr::memory_resource *mr = nullptr);

/// squareform(d) — convert a pdist row to a symmetric square matrix
/// (or vice versa: square → pdist row).
Value squareform(const Value &d, std::pmr::memory_resource *mr = nullptr);

/// mahal(Y, X) — Mahalanobis distance² from each row of Y to the mean
/// of X using the sample covariance of X. Returns N×1 column.
Value mahal(const Value &Y, const Value &X, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
