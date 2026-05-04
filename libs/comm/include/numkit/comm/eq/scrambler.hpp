// libs/comm/include/numkit/comm/eq/scrambler.hpp
//
// Multiplicative bit scrambler / descrambler — the function-form
// equivalent of MATLAB's comm.Scrambler / comm.Descrambler System
// Objects. Both use the same shift-register feedback structure;
// the descrambler is the algebraic inverse of the scrambler so a
// round-trip recovers the original bit sequence exactly when both
// sides share the same polynomial and initial state.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// `y = scrambler(x, poly, initState)` — multiplicative scrambler.
/// `x` is a column vector of bits (0/1, double or logical).
/// `poly` is a length-(n+1) coefficient vector `[g_0, g_1, …, g_n]`
/// with g_0 = 1; non-zero entries mark register taps. `initState`
/// is the length-n initial register contents (bits 0/1).
/// Output is a length-(numel x) column of doubles in {0, 1}.
Value scrambler(std::pmr::memory_resource *mr,
                const Value &x, const Value &poly,
                const Value &initState);

/// `x = descrambler(y, poly, initState)` — algebraic inverse of
/// `scrambler`. Same arguments; round-trip recovers the input bit
/// sequence exactly.
Value descrambler(std::pmr::memory_resource *mr,
                  const Value &y, const Value &poly,
                  const Value &initState);

} // namespace numkit::comm
