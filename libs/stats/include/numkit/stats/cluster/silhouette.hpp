// libs/stats/include/numkit/stats/cluster/silhouette.hpp
//
// Silhouette coefficient — cluster-validity score per point.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit::stats {

/// silhouette(X, clust [, metric [, p]]) — silhouette coefficient
/// per row of X. Default metric is "sqeuclidean" (matches MATLAB
/// R2025b's silhouette).
///
/// For each point i with label k:
///   a(i) = mean distance to other points with label k (or 0 if none)
///   b(i) = min over labels k' ≠ k of mean distance from i to cluster k'
///   s(i) = (b(i) - a(i)) / max(a(i), b(i))
/// Singleton clusters are assigned s(i) = 0.
///
/// Output: column vector of length size(X, 1).
Value silhouette(const Value &X, const Value &clust, const std::string &metric = "sqeuclidean", double p = 2.0, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
