// libs/comm/include/numkit/comm/coding/convcoding.hpp
//
// Convolutional coding — Error Correction Codes section of the
// Communications Toolbox: poly2trellis (and, as they land, istrellis /
// convenc / vitdec).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// @brief Convolutional-code trellis from polynomial generators
/// (`trellis = poly2trellis(ConstraintLength, CodeGenerator)`).
///
/// Builds the MATLAB trellis struct describing a rate-1/n feed-forward
/// convolutional encoder. Returns a 1×1 struct with fields (in MATLAB's
/// order):
///   - `numInputSymbols`  = `2`            (one input bit, k = 1)
///   - `numOutputSymbols` = `2^n`          (n = numel(CodeGenerator))
///   - `numStates`        = `2^(K-1)`      (K = ConstraintLength)
///   - `nextStates`       = `numStates × 2` next-state index per (state, input)
///   - `outputs`          = `numStates × 2` output word (decimal of the
///                          n output bits, first generator = MSB)
///
/// `CodeGenerator` entries are octal numbers (e.g. `[6 7]`, `[171 133]`).
/// The shift-register state holds the `K-1` previous input bits; for each
/// `(state, input)` the K-bit register `reg = (input << (K-1)) | state`
/// drives `output_i = parity(reg & g_i)` and `nextState = reg >> 1`.
///
/// v1 scope: rate 1/n (scalar `ConstraintLength`, length-n `CodeGenerator`),
/// feed-forward only. Rate k/n (vector `ConstraintLength`, k×n generator
/// matrix) and feedback connections are a documented gap.
///
/// @param constraintLength  Scalar constraint length `K` (`>= 1`).
/// @param codeGenerator      Length-n vector of octal generator polynomials.
/// @param mr                 Memory resource (nullptr → process default).
/// @return                   1×1 trellis struct.
/// @throws Error on a non-scalar `constraintLength` (rate k/n), `K < 1`, an
///         empty `codeGenerator`, or an octal digit `> 7`.
Value poly2trellis(const Value &constraintLength, const Value &codeGenerator,
                   std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
