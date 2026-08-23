/// @file distance.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/cluster/distance.hpp
//
// Pairwise distance utilities. These primitives feed every clustering
// algorithm in the toolbox (kmeans, kmedoids, linkage, knnsearch, …).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>

namespace numkit::stats {

/// @addtogroup group_stats
/// @{


/// Pairwise distances between rows of X (`d = pdist(X, metric, p)`).
///
/// Returns a `1 × M(M-1)/2` row (upper-triangular packed). Supported
/// metrics:
///
///   | metric              | formula                                 |
///   | ------------------- | --------------------------------------- |
///   | `"euclidean"` (def) | √Σ(xᵢ − yᵢ)²                            |
///   | `"squaredeuclidean"`| Σ(xᵢ − yᵢ)²                             |
///   | `"cityblock"`       | Σ|xᵢ − yᵢ|                              |
///   | `"chebychev"`       | max|xᵢ − yᵢ|                            |
///   | `"minkowski"`       | (Σ|xᵢ − yᵢ|^p)^(1/p) — uses `p`         |
///   | `"cosine"`          | 1 − x·y / (‖x‖‖y‖) (NaN if a norm is 0) |
///   | `"correlation"`     | 1 − corr(x, y) (NaN if a row is const)  |
///   | `"seuclidean"`      | √Σ((xᵢ−yᵢ)/Sᵢ)², Sᵢ=std(col) by default |
///   | `"spearman"`        | 1 − corr of tied ranks of each row      |
///   | `"hamming"`         | mean(x ≠ y)                             |
///   | `"jaccard"`         | mean(x ≠ y given at least one nonzero)  |
///
/// @param X       M×D matrix (each row a point).
/// @param metric  Metric name (see table).
/// @param p       Minkowski exponent (only used for `"minkowski"`).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Row of length M(M-1)/2.
///
/// @see pdist2, squareform
Value pdist(const Value &X, const std::string &metric, double p,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Cross-set pairwise distances
/// (`D = pdist2(X, Y, metric, p)`).
///
/// Distances between rows of `X` (M×D) and rows of `Y` (N×D); output
/// is M×N. Metric and `p` follow @ref pdist.
///
/// @param X       M×D matrix.
/// @param Y       N×D matrix.
/// @param metric  Metric name (see @ref pdist).
/// @param p       Minkowski exponent.
/// @param mr      Memory resource (nullptr → process default).
/// @return        M×N distance matrix.
/// @see pdist
Value pdist2(const Value &X, const Value &Y, const std::string &metric, double p,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Pack / unpack the pdist row form (`Y = squareform(d)`).
///
/// Converts a pdist row into a symmetric square matrix, or a square
/// matrix back into a pdist row.
///
/// @param d   pdist row OR symmetric square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Square matrix or pdist row, the other shape.
/// @see pdist
Value squareform(const Value &d,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Mahalanobis distance squared (`d² = mahal(Y, X)`).
///
/// For each row of `Y`, computes
/// `d² = (y − x̄) · S⁻¹ · (y − x̄)ᵀ`
/// where `S` is the sample covariance of `X` and `x̄` its mean.
///
/// @param Y   N×D query rows.
/// @param X   M×D reference rows defining the covariance.
/// @param mr  Memory resource (nullptr → process default).
/// @return    N×1 column of squared Mahalanobis distances.
Value mahal(const Value &Y, const Value &X,
            std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::stats
