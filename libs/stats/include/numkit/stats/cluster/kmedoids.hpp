// libs/stats/include/numkit/stats/cluster/kmedoids.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::stats {

/// kmedoids(X, K[, MaxIter, Replicates, metric]) — PAM-style k-medoids.
/// Returns (idx, medoids, sumd) where medoids is K×D from rows of X.
std::tuple<Value, Value, Value>
kmedoids(std::pmr::memory_resource *mr, const Value &X, int K,
         int max_iter, int replicates, const std::string &metric);

/// dbscan(X, eps, minpts[, metric]) — density-based clustering.
/// Returns (idx, corepts) where idx is N×1 cluster labels (≥1 for
/// clusters, 0 for noise — MATLAB convention) and corepts is N×1
/// LOGICAL marking core points.
std::tuple<Value, Value>
dbscan(std::pmr::memory_resource *mr, const Value &X,
       double eps, int minpts, const std::string &metric);

} // namespace numkit::stats
