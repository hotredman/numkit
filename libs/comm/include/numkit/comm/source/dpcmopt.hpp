// libs/comm/include/numkit/comm/source/dpcmopt.hpp
//
// DPCM parameter optimiser.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

struct DpcmOptResult {
    Value predictor;
    Value codebook;   // empty if not requested
    Value partition;  // empty if not requested
};

/// `[predictor, codebook, partition] = dpcmopt(training, ord [, ini_codebook])`
///   - predictor: length-(ord+1) row vector `[0, p1, ..., pM]` from
///     Levinson-Durbin solution to the Yule-Walker autocorrelation
///     system, then negated to match dpcmenco's convention.
///   - codebook + partition: optional outputs from running lloyds()
///     on the prediction residual, requires ini_codebook (integer K
///     or explicit initial codebook vector).
DpcmOptResult
dpcmopt(std::pmr::memory_resource *mr, const Value &training_set,
        int ord, const Value *ini_codebook);

} // namespace numkit::comm
