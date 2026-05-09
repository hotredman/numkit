// libs/comm/include/numkit/comm/source/quantiz.hpp
//
// Scalar quantizer applier.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <utility>

namespace numkit::comm {

/// `indx = quantiz(sig, partition)` — bin index per sample.
///   indx(i) = sum(partition < sig(i)) ∈ [0, length(partition)]
/// Output preserves sig orientation.
Value quantiz_indx(std::pmr::memory_resource *mr, const Value &sig,
                   const Value &partition);

/// `[indx, quantv (, distor)] = quantiz(sig, partition, codebook)` —
/// also returns the codebook value selected for each sample
/// (`codebook(indx + 1)`) and, if `quantv_out` is non-null, the
/// distortion = mean((sig - quantv)^2). Returns {indx, distor}.
std::pair<Value, double>
quantiz_distor(std::pmr::memory_resource *mr, const Value &sig,
               const Value &partition, const Value &codebook,
               Value *quantv_out);

} // namespace numkit::comm
