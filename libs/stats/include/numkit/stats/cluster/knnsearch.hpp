// libs/stats/include/numkit/stats/cluster/knnsearch.hpp
//
// Brute-force k-nearest-neighbor and radius search.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::stats {

/// `[Idx, D] = knnsearch(X, Y, K, metric, p)` — for each row of Y find
/// the K nearest rows of X under the given metric (defaults: K=1,
/// metric="euclidean", p=2 for Minkowski). Returns:
///   Idx — M_Y × K matrix of 1-based row indices into X
///   D   — M_Y × K matrix of the matching distances
/// Ties are broken by ascending row index. Brute force, no kd-tree.
std::tuple<Value, Value>
knnsearch(std::pmr::memory_resource *mr,
          const Value &X, const Value &Y,
          int K = 1,
          const std::string &metric = "euclidean",
          double p = 2.0);

/// `[Idx, D] = rangesearch(X, Y, r, metric, p)` — for each row of Y
/// list every row of X within distance r. Returns:
///   Idx — M_Y × 1 cell array; each cell is a 1×n_i row of 1-based
///         indices sorted by ascending distance
///   D   — M_Y × 1 cell array; matching distances
/// Default metric "euclidean", p=2.
std::tuple<Value, Value>
rangesearch(std::pmr::memory_resource *mr,
            const Value &X, const Value &Y, double r,
            const std::string &metric = "euclidean",
            double p = 2.0);

} // namespace numkit::stats
