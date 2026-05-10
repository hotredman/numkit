// libs/comm/include/numkit/comm/source/arithcoding.hpp
//
// Arithmetic coding encoder + decoder.

#pragma once

#include <cstddef>
#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// `code = arithenco(seq, counts)` — arithmetic-encode a symbol
/// sequence using the cumulative-probability intervals derived
/// from `counts` (positive integer vector). Output is a 0/1 bit
/// vector; orientation matches `seq`.
Value arithenco(std::pmr::memory_resource *mr, const Value &seq,
                const Value &counts);

/// `seq = arithdeco(code, counts, len)` — invert arithenco.
/// `len` gives the expected decoded length (the encoder doesn't
/// emit a stop sentinel; `len` was MATLAB's contract too).
Value arithdeco(std::pmr::memory_resource *mr, const Value &code,
                const Value &counts, std::size_t len);

} // namespace numkit::comm
