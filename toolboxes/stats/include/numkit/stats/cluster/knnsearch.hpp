// toolboxes/stats/include/numkit/stats/cluster/knnsearch.hpp
//
// Brute-force k-nearest-neighbor and radius search.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>
#include <tuple>

namespace numkit::stats {

/// k-nearest-neighbour search (`[Idx, D] = knnsearch(X, Y, K, metric, p)`).
///
/// For each row of `Y` find the `K` nearest rows of `X` under the
/// given metric. Brute-force scan (no kd-tree). Ties are broken by
/// ascending row index.
///
/// @param X       Reference points (M_X × D).
/// @param Y       Query points     (M_Y × D).
/// @param K       Neighbour count (≥ 1; default 1).
/// @param metric  @ref pdist metric (default `"euclidean"`).
/// @param p       Minkowski exponent (default 2).
/// @param mr      Memory resource (nullptr → process default).
/// @return        `(Idx, D)`:
///                  - `Idx` : M_Y × K row indices into `X` (1-based).
///                  - `D`   : M_Y × K matching distances.
///
/// @see rangesearch, pdist
std::tuple<Value, Value>
knnsearch(const Value &X, const Value &Y, int K = 1,
          const std::string &metric = "euclidean", double p = 2.0,
          std::pmr::memory_resource *mr = nullptr);

/// Radius search (`[Idx, D] = rangesearch(X, Y, r, metric, p)`).
///
/// For each row of `Y` list every row of `X` within distance `r` of it.
/// Sorted by ascending distance.
///
/// @param X       Reference points.
/// @param Y       Query points.
/// @param r       Search radius.
/// @param metric  @ref pdist metric (default `"euclidean"`).
/// @param p       Minkowski exponent (default 2).
/// @param mr      Memory resource (nullptr → process default).
/// @return        `(Idx, D)`:
///                  - `Idx` : M_Y × 1 cell array; each cell is a row
///                            of 1-based indices.
///                  - `D`   : M_Y × 1 cell array of distances.
///
/// @see knnsearch
std::tuple<Value, Value>
rangesearch(const Value &X, const Value &Y, double r,
            const std::string &metric = "euclidean", double p = 2.0,
            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
