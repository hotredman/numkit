// libs/signal/include/numkit/signal/digital_filtering/quantizers.hpp
//
// Uniform-quantization helpers (Phase 4.2):
//   uencode — float → integer (signed/unsigned, N bits, peak V)
//   udecode — integer → float (saturate/wrap on overflow)

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// @brief Quantise and encode `u` as N-bit integers (`y = uencode(u, N, V, type)`).
///
/// @param u             Real input.
/// @param N             Bit width in `[2, 32]`.
/// @param V             Peak input level (saturates at ±V).
/// @param signedOutput  false → uint8/uint16/uint32; true → int8/int16/int32.
///                      Output container type follows the least-bits
///                      rule (N≤8 → 8-bit, N≤16 → 16-bit, else 32-bit).
/// @param mr            Memory resource (nullptr → process default).
/// @return              Integer-typed Value, same shape as `u`.
/// @see udecode
Value uencode(const Value &u, int N, double V = 1.0,
              bool signedOutput = false,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Decode integer data back to double with peak ±V
/// (`y = udecode(u, N, V, overflow)`).
///
/// @param u               Input (int8/16/32 or uint8/16/32).
/// @param N               Bit width used at encoding time.
/// @param V               Peak output level.
/// @param wrapOnOverflow  false → saturate; true → wrap modulo 2^N.
/// @param mr              Memory resource (nullptr → process default).
/// @return                DOUBLE Value, same shape as `u`.
/// @see uencode
Value udecode(const Value &u, int N, double V = 1.0,
              bool wrapOnOverflow = false,
              std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
