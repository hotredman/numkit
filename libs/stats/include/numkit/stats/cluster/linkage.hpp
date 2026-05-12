// libs/stats/include/numkit/stats/cluster/linkage.hpp
//
// Agglomerative hierarchical clustering — linkage tree + flat-cluster
// extraction. Function-form API only.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::stats {

/// linkage(Y[, method]) — build agglomerative tree from pdist output Y.
///   method: "single" (default), "complete", "average", "weighted",
///           "centroid", "median", "ward"
/// Returns Z, an (N-1)×3 array where row i = [a, b, d]:
///   a, b — 1-based ids of the two merged clusters
///   d    — distance at merge
/// Cluster ids: 1..N for original points; N+1, N+2, ... for new clusters
/// formed by each merge.
Value linkage(const Value &Y, const std::string &method, std::pmr::memory_resource *mr = nullptr);
/// 4-arg overload: when Y is a raw N×D data matrix, computes pdist(Y,
/// metric, p) internally instead of defaulting to euclidean.
Value linkage(const Value &Y, const std::string &method, const std::string &metric, double p, std::pmr::memory_resource *mr = nullptr);

/// cluster(Z, opts) — flatten a linkage tree into cluster labels.
///   opts.maxclust  — cap number of clusters (cuts at lowest distance
///                    that yields ≤ maxclust clusters)
///   opts.cutoff    — cut at given inconsistency threshold (or distance
///                    if 'criterion' is 'distance')
/// Returns N×1 column of 1-based cluster labels.
Value cluster_from_linkage(const Value &Z, int maxclust, double cutoff, const std::string &criterion, std::pmr::memory_resource *mr = nullptr);
Value cluster_from_linkage(const Value &Z, int maxclust, double cutoff, const std::string &criterion, int depth, std::pmr::memory_resource *mr = nullptr);

/// clusterdata(X, …) — convenience: pdist + linkage + cluster.
Value clusterdata(const Value &X, int maxclust, double cutoff, const std::string &linkage_method, const std::string &criterion, std::pmr::memory_resource *mr = nullptr);
Value clusterdata(const Value &X, int maxclust, double cutoff, const std::string &linkage_method, const std::string &criterion, int depth, std::pmr::memory_resource *mr = nullptr);
Value clusterdata(const Value &X, int maxclust, double cutoff, const std::string &linkage_method, const std::string &criterion, int depth, const std::string &distance_metric, double p, std::pmr::memory_resource *mr = nullptr);

/// cophenet(Z, Y) — cophenetic correlation coefficient between linkage
/// distances and original pdist distances. Returns a scalar.
Value cophenet(const Value &Z, const Value &Y, std::pmr::memory_resource *mr = nullptr);

/// inconsistent(Z[, depth]) — inconsistency coefficient table.
/// Returns (N-1)×4 array: [mean, std, count, inconsistency].
Value inconsistent(const Value &Z, int depth, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
