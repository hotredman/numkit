// libs/comm/include/numkit/comm/source/dpcm.hpp
//
// Differential Pulse Code Modulation encoder/decoder.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <utility>

namespace numkit::comm {

/// @brief DPCM encoder
/// (`[indx, quanterr] = dpcmenco(sig, codebook, partition, predictor)`).
///
/// The predictor's leading element is a sentinel 0; the
/// remaining M elements are the FIR coefficients.
/// `length(codebook) == length(partition) + 1`.
///
/// @param sig        Real input vector.
/// @param codebook   Quantization codebook (length K).
/// @param partition  Strictly increasing partition (length K − 1).
/// @param predictor  `[0, p1, …, pM]`.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Tuple `(indx, quanterr)` — index sequence and
///                   quantisation error per sample.
/// @see dpcmdeco, dpcmopt
std::pair<Value, Value>
dpcmenco(const Value &sig, const Value &codebook,
         const Value &partition, const Value &predictor,
         std::pmr::memory_resource *mr = nullptr);

/// @brief DPCM decoder — invert @ref dpcmenco
/// (`[sig, quanterr] = dpcmdeco(indx, codebook, predictor)`).
///
/// Recovers the signal from the index sequence using the same
/// `codebook` and `predictor` as the encoder.
///
/// @param indx       Index sequence from @ref dpcmenco.
/// @param codebook   Quantization codebook (length K).
/// @param predictor  Same `[0, p1, …, pM]` used at encode time.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Tuple `(sig, quanterr)`.
/// @see dpcmenco
std::pair<Value, Value>
dpcmdeco(const Value &indx, const Value &codebook,
         const Value &predictor,
         std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
