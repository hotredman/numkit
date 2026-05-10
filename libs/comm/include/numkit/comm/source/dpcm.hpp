// libs/comm/include/numkit/comm/source/dpcm.hpp
//
// Differential Pulse Code Modulation encoder/decoder.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <utility>

namespace numkit::comm {

/// `[indx, quanterr] = dpcmenco(sig, codebook, partition, predictor)` —
/// encode `sig` via DPCM. The predictor's leading element is the
/// MATLAB sentinel 0; remaining M elements are the FIR coefficients.
/// `length(codebook) == length(partition) + 1`.
std::pair<Value, Value>
dpcmenco(std::pmr::memory_resource *mr, const Value &sig,
         const Value &codebook, const Value &partition,
         const Value &predictor);

/// `[sig, quanterr] = dpcmdeco(indx, codebook, predictor)` — invert
/// dpcmenco using the same `codebook` and `predictor`.
std::pair<Value, Value>
dpcmdeco(std::pmr::memory_resource *mr, const Value &indx,
         const Value &codebook, const Value &predictor);

} // namespace numkit::comm
