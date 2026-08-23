/// @file kmeans.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/cluster/kmeans.hpp

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit {

/// @addtogroup group_stats
/// @{
 namespace ops { class RngContext; } }

namespace numkit::stats {

/// Lloyd's k-means clustering (`[idx, C, sumd] = kmeans(X, K, …)`).
///
/// Standard Euclidean k-means with random initial seeding. Multiple
/// restarts are run and the lowest-distortion solution is returned.
///
/// @param X           N×D data matrix (one observation per row).
/// @param K           Number of clusters (≥ 1).
/// @param max_iter    Maximum Lloyd iterations (default 100).
/// @param replicates  Number of random restarts; the best (lowest sum
///                    of squared distances) is returned (default 1).
/// @param mr          Memory resource (nullptr → process default).
/// @return            `(idx, C, sumd)`:
///                      - `idx`  : N×1 cluster assignments in {1, …, K}.
///                      - `C`    : K×D cluster centroids.
///                      - `sumd` : K×1 within-cluster sum of squared distances.
///
/// @see kmedoids, linkage
std::tuple<Value, Value, Value>
kmeans(::numkit::ops::RngContext &rng, const Value &X, int K, int max_iter, int replicates,
       std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::stats
