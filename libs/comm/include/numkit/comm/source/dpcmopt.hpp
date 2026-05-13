// libs/comm/include/numkit/comm/source/dpcmopt.hpp
//
// DPCM parameter optimiser.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// @brief Result of @ref dpcmopt.
struct DpcmOptResult {
    Value predictor;  ///< Length-(ord+1) row vector `[0, p1, …, pM]`.
    Value codebook;   ///< Optional codebook — empty unless requested.
    Value partition;  ///< Optional partition — empty unless requested.
};

/// @brief Optimise DPCM predictor + (optional) quantizer
/// (`[predictor, codebook, partition] = dpcmopt(training, ord, ini_codebook)`).
///
/// - `predictor`: length-(ord+1) row vector `[0, p1, …, pM]` obtained
///   from the Levinson-Durbin solution to the Yule-Walker
///   autocorrelation system, then negated to match @ref dpcmenco's
///   convention.
/// - `codebook` + `partition`: optional outputs from running
///   @ref lloyds on the prediction residual; emitted only when
///   `ini_codebook` is supplied (integer K or explicit initial
///   codebook vector).
///
/// @param training_set  Training signal (real vector).
/// @param ord           Predictor order ≥ 0.
/// @param ini_codebook  Optional initial codebook (or codebook size).
///                      Pass `Value::Empty` to skip codebook design.
/// @param mr            Memory resource (nullptr → process default).
/// @return              @ref DpcmOptResult with the new predictor and
///                      (optionally) codebook + partition.
/// @see dpcmenco, dpcmdeco, lloyds
DpcmOptResult
dpcmopt(const Value &training_set, int ord,
        const Value &ini_codebook = Value::Empty,
        std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
