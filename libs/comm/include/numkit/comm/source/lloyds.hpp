// libs/comm/include/numkit/comm/source/lloyds.hpp
//
// Lloyd-Max scalar quantizer designer.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <tuple>

namespace numkit::comm {

/// @brief Lloyd-Max scalar quantizer designer
/// (`[partition, codebook, distor, rel] = lloyds(training, ini_codebook, tol)`).
///
/// Iteratively alternates between optimal partition (mid-points
/// between adjacent codebook entries) and optimal codebook (centroid
/// of each cell) until the relative-distortion change drops below
/// `tol`.
///
/// @param training_set  Non-empty vector of real samples.
/// @param ini_codebook  Either a positive integer K (initialise K
///                      linspace-centred codebook entries spanning
///                      the training range) or an explicit codebook
///                      vector (sorted ascending internally).
/// @param tol           Convergence tolerance on the relative
///                      distortion change. Default 1e-7.
/// @param mr            Memory resource (nullptr → process default).
/// @return              Tuple `(partition, codebook, distor, rel)`:
///                      final partition + codebook, achieved
///                      distortion, and last-step relative change.
/// @throws Error        On empty training set or invalid
///                      `ini_codebook`.
/// @see quantiz, dpcmopt
std::tuple<Value, Value, double, double>
lloyds(const Value &training_set, const Value &ini_codebook,
       double tol = 1e-7,
       std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
