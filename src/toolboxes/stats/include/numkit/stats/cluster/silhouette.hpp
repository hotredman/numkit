/// @file silhouette.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/cluster/silhouette.hpp
//
// Silhouette coefficient — cluster-validity score per point.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>

namespace numkit::stats {

/// @addtogroup group_stats
/// @{


/// @brief Per-point silhouette coefficient (`s = silhouette(X, clust, metric, p)`).
///
/// For each point `i` with label `k`, computes:
/// - `a(i)` = mean distance to other points with label `k` (0 if singleton)
/// - `b(i)` = `min` over labels `k' ≠ k` of the mean distance from `i`
///   to cluster `k'`
/// - `s(i) = (b(i) - a(i)) / max(a(i), b(i))`
///
/// Singleton clusters are assigned `s(i) = 0`.
///
/// @param X       `N × d` data matrix (one observation per row).
/// @param clust   `N × 1` integer cluster labels.
/// @param metric  Distance metric — see @ref pdist (default `"sqeuclidean"`).
/// @param p       Minkowski exponent (used only for `metric == "minkowski"`,
///                default 2).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Column vector of silhouette coefficients in `[-1, 1]`.
/// @see pdist, kmeans
Value silhouette(const Value &X, const Value &clust,
                 const std::string &metric = "sqeuclidean", double p = 2.0,
                 std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::stats
