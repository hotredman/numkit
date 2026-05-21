// libs/comm/include/numkit/comm/eq/scrambler.hpp
//
// Multiplicative bit scrambler / descrambler — the function-form
// equivalent of the comm.Scrambler / comm.Descrambler System
// Objects.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// @brief Multiplicative bit scrambler
/// (`y = scrambler(x, poly, initState)`).
///
/// Per-bit recursion:
/// - `fb = XOR over i=1..n where g_i = 1 of state[i-1]`
/// - `y[k] = x[k] XOR fb`
/// - shift state right by 1, store `y[k]` at `state[0]`
///
/// Both scrambler and @ref descrambler clock the **channel** bit
/// `y[k]` into the shift register, which is what makes the
/// descrambler self-synchronizing.
///
/// @param x          Column of input bits (0/1, DOUBLE or LOGICAL).
/// @param poly       Length-(n+1) coefficient vector
///                   `[g_0, g_1, …, g_n]` with `g_0 = 1`; non-zero
///                   entries mark register taps.
/// @param initState  Length-n initial register contents (bits 0/1).
/// @param mr         Memory resource (nullptr → process default).
/// @return           Column of `numel(x)` DOUBLE bits in {0, 1}.
/// @throws Error     On bad poly (length, `g_0 = 0`) or state size.
/// @see descrambler
Value scrambler(const Value &x, const Value &poly,
                const Value &initState,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Multiplicative bit descrambler — algebraic inverse of
/// @ref scrambler (`x = descrambler(y, poly, initState)`).
///
/// Per-bit recursion:
/// - `fb = XOR over i=1..n where g_i = 1 of state[i-1]`
/// - `x[k] = y[k] XOR fb`
/// - shift state right by 1, store `y[k]` (channel bit) at
///   `state[0]` — **not** `x[k]`.
///
/// Round-trip with @ref scrambler using the same `poly` and
/// `initState` recovers the original bit sequence exactly.
///
/// @param y          Column of scrambled bits.
/// @param poly       Same coefficient vector as the scrambler.
/// @param initState  Same initial state as the scrambler.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Column of recovered DOUBLE bits in {0, 1}.
/// @throws Error     On bad poly or state size.
/// @see scrambler
Value descrambler(const Value &y, const Value &poly,
                  const Value &initState,
                  std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
