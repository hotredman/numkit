// libs/comm/include/numkit/comm/source/lloyds.hpp
//
// Lloyd-Max scalar quantizer designer.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <tuple>

namespace numkit::comm {

/// `[partition, codebook, distor, rel] = lloyds(training, ini_codebook
/// [, tol])` — Lloyd-Max scalar quantizer designer.
///
///   training     : non-empty vector of real samples.
///   ini_codebook : either a positive integer K (initialise K linspace-
///                  centred codebook entries spanning the training range)
///                  or an explicit codebook vector (sorted ascending
///                  internally).
///   tol          : convergence tolerance on relative-distortion change.
///                  Default 1e-7.
std::tuple<Value, Value, double, double>
lloyds(std::pmr::memory_resource *mr, const Value &training_set,
       const Value &ini_codebook, double tol);

} // namespace numkit::comm
