// libs/comm/include/numkit/comm/source/quantiz.hpp
//
// Scalar quantizer applier.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// @brief One-output form of `quantiz` — bin index per sample
/// (`indx = quantiz(sig, partition)`).
///
/// `indx(i) = sum(partition < sig(i))`, in the range
/// `[0, length(partition)]`. Output preserves `sig`'s orientation.
///
/// @param sig        Input signal (real vector).
/// @param partition  Strictly increasing partition vector.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Bin index per sample.
/// @see quantiz, lloyds
Value quantiz_indx(const Value &sig, const Value &partition,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief Result of the three-output form of MATLAB's `quantiz`.
struct QuantizResult {
    Value  indx;     ///< Bin index per sample.
    Value  quantv;   ///< `codebook(indx + 1)`.
    double distor;   ///< `mean((sig - quantv)^2)`.
};

/// @brief Three-output form of `quantiz`
/// (`[indx, quantv, distor] = quantiz(sig, partition, codebook)`).
///
/// @param sig        Input signal.
/// @param partition  Strictly increasing partition vector (length K-1).
/// @param codebook   Codebook vector (length K).
/// @param mr         Memory resource (nullptr → process default).
/// @return           @ref QuantizResult with index, quantised values,
///                   and mean-square distortion.
/// @see quantiz_indx, lloyds
QuantizResult
quantiz(const Value &sig, const Value &partition, const Value &codebook,
        std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
