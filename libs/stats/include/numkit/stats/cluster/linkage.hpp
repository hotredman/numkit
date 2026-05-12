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

/// Agglomerative hierarchical clustering tree (`Z = linkage(Y, method)`).
///
/// Builds an N−1 × 3 linkage table from a pdist row `Y`. Row i is
/// `[a, b, d]`: ids `a, b` of the two clusters merged at step i, and
/// the distance `d` at the merge. Cluster ids: `1..N` for original
/// points, `N+1, N+2, …` for new clusters formed by each merge.
///
/// Supported `method`:
/// `"single"` (default), `"complete"`, `"average"`, `"weighted"`,
/// `"centroid"`, `"median"`, `"ward"`.
///
/// @param Y       pdist-row vector (`1 × N(N-1)/2`).
/// @param method  Linkage rule (see list above).
/// @param mr      Memory resource (nullptr → process default).
/// @return        (N−1)×3 linkage table.
///
/// @see cluster_from_linkage, cophenet, inconsistent
Value linkage(const Value &Y, const std::string &method,
              std::pmr::memory_resource *mr = nullptr);

/// 4-arg overload: when `Y` is a raw N×D data matrix, computes
/// `pdist(Y, metric, p)` internally before linkage.
Value linkage(const Value &Y, const std::string &method,
              const std::string &metric, double p,
              std::pmr::memory_resource *mr = nullptr);

/// Flatten a linkage tree into cluster labels
/// (`T = cluster(Z, opts)`).
///
/// @param Z          Linkage table from @ref linkage.
/// @param maxclust   Cap number of clusters (cuts at the lowest
///                   distance that yields ≤ `maxclust` clusters).
///                   Pass 0 to ignore.
/// @param cutoff     Cut at given inconsistency / distance threshold.
/// @param criterion  `"inconsistent"` (default) or `"distance"`.
/// @param mr         Memory resource (nullptr → process default).
/// @return           N×1 column of 1-based cluster labels.
Value cluster_from_linkage(const Value &Z, int maxclust, double cutoff,
                           const std::string &criterion,
                           std::pmr::memory_resource *mr = nullptr);

/// 5-arg overload: explicit inconsistency depth `depth`.
Value cluster_from_linkage(const Value &Z, int maxclust, double cutoff,
                           const std::string &criterion, int depth,
                           std::pmr::memory_resource *mr = nullptr);

/// Convenience: pdist + linkage + cluster in one call (`clusterdata`).
///
/// @param X                Raw data matrix (N×D).
/// @param maxclust         See @ref cluster_from_linkage.
/// @param cutoff           See @ref cluster_from_linkage.
/// @param linkage_method   Linkage rule.
/// @param criterion        Threshold criterion.
/// @param mr               Memory resource (nullptr → process default).
/// @return                 N×1 cluster labels.
Value clusterdata(const Value &X, int maxclust, double cutoff,
                  const std::string &linkage_method,
                  const std::string &criterion,
                  std::pmr::memory_resource *mr = nullptr);

/// 6-arg overload: explicit inconsistency depth.
Value clusterdata(const Value &X, int maxclust, double cutoff,
                  const std::string &linkage_method,
                  const std::string &criterion, int depth,
                  std::pmr::memory_resource *mr = nullptr);

/// 8-arg overload: explicit depth + custom distance metric.
Value clusterdata(const Value &X, int maxclust, double cutoff,
                  const std::string &linkage_method,
                  const std::string &criterion, int depth,
                  const std::string &distance_metric, double p,
                  std::pmr::memory_resource *mr = nullptr);

/// Cophenetic correlation (`c = cophenet(Z, Y)`).
///
/// Correlation between linkage distances (in `Z`) and the original
/// pdist distances `Y`. A measure of how faithfully the tree
/// represents the input dissimilarities; higher = better.
Value cophenet(const Value &Z, const Value &Y,
               std::pmr::memory_resource *mr = nullptr);

/// Inconsistency coefficient table (`R = inconsistent(Z, depth)`).
///
/// Returns an (N−1)×4 array `[mean, std, count, inconsistency]`. Use
/// with @ref cluster_from_linkage and `criterion = "inconsistent"`.
Value inconsistent(const Value &Z, int depth,
                   std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
