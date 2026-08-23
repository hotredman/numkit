/// @file kmedoids.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/cluster/kmedoids.hpp

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>
#include <tuple>

namespace numkit {

/// @addtogroup group_stats
/// @{
 namespace ops { class RngContext; } }

namespace numkit::stats {

/// PAM-style k-medoids clustering (`[idx, medoids, sumd] = kmedoids(...)`).
///
/// Like @ref kmeans but cluster centres are required to be **data
/// points** (medoids), so the algorithm works with any distance
/// metric (`metric` argument follows @ref pdist).
///
/// @param X           N×D data matrix.
/// @param K           Number of clusters.
/// @param max_iter    Maximum PAM iterations.
/// @param replicates  Random restarts; best (lowest total distance) wins.
/// @param metric      Distance metric (see @ref pdist).
/// @param mr          Memory resource (nullptr → process default).
/// @return            `(idx, medoids, sumd)` — analogous to @ref kmeans.
///
/// @see kmeans, dbscan
std::tuple<Value, Value, Value>
kmedoids(::numkit::ops::RngContext &rng, const Value &X, int K, int max_iter, int replicates,
         const std::string &metric,
         std::pmr::memory_resource *mr = nullptr);

/// DBSCAN density-based clustering (`[idx, corepts] = dbscan(X, eps, minpts, metric)`).
///
/// Builds clusters from `corepts` — points with at least `minpts`
/// neighbours within distance `eps`. Output labels: cluster index ≥ 1,
/// or -1 for noise.
///
/// Pass `metric = "precomputed"` to use `X` as a pre-built N×N
/// pairwise distance matrix directly.
///
/// @param X        N×D data matrix or N×N precomputed distances.
/// @param eps      Neighbourhood radius.
/// @param minpts   Minimum neighbour count for a core point.
/// @param metric   @ref pdist metric, or `"precomputed"`.
/// @param mr       Memory resource (nullptr → process default).
/// @return         `(idx, corepts)`:
///                   - `idx`     : N×1 cluster labels (≥1 or -1 for noise).
///                   - `corepts` : N×1 LOGICAL marking core points.
///
/// @see kmedoids, kmeans
std::tuple<Value, Value>
dbscan(const Value &X, double eps, int minpts, const std::string &metric,
       std::pmr::memory_resource *mr = nullptr);

/// DBSCAN with explicit Minkowski exponent `p` (for `metric = "minkowski"`).
///
/// @see dbscan(X, eps, minpts, metric, mr)
std::tuple<Value, Value>
dbscan(const Value &X, double eps, int minpts, const std::string &metric,
       double p,
       std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::stats
