// libs/comm/include/numkit/comm/source/quantiz.hpp
//
// Scalar quantizer applier.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// `indx = quantiz(sig, partition)` — bin index per sample.
///   indx(i) = sum(partition < sig(i)) ∈ [0, length(partition)]
/// Output preserves sig orientation.
Value quantiz_indx(const Value &sig, const Value &partition,
                   std::pmr::memory_resource *mr = nullptr);

/// Result of MATLAB `[indx, quantv, distor] = quantiz(sig, partition, codebook)`.
struct QuantizResult {
    Value  indx;     ///< bin index per sample
    Value  quantv;   ///< codebook(indx + 1)
    double distor;   ///< mean((sig - quantv)^2)
};

/// `[indx, quantv, distor] = quantiz(sig, partition, codebook)`.
QuantizResult
quantiz(const Value &sig, const Value &partition, const Value &codebook,
        std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
