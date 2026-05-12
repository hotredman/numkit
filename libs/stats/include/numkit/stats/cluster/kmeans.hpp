// libs/stats/include/numkit/stats/cluster/kmeans.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// kmeans(X, K[, MaxIter, Replicates]) — Lloyd's k-means.
///   X:           N×D data matrix
///   K:           number of clusters
///   MaxIter:     iteration cap (default 100)
///   Replicates:  how many random restarts to keep the best of (default 1)
///
/// Returns (idx, C, sumd):
///   idx:  N×1 cluster assignments (1..K)
///   C:    K×D cluster centroids
///   sumd: K×1 within-cluster sum of squared distances
std::tuple<Value, Value, Value>
kmeans(const Value &X, int K, int max_iter, int replicates, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
