// libs/builtin/include/numkit/builtin/language/bitwise/int_math.hpp
//
// Phase-7 integer-flavored numeric utilities: gcd / lcm / bitwise ops.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// @file
/// @brief Integer-flavoured arithmetic and bitwise operations.
///
/// All inputs are read as doubles, converted to `int64_t` for the
/// operation, and written back as doubles. This suits
/// "small integer" workflows without forcing callers
/// into explicit integer casts. Values outside `[-2^53, 2^53]`
/// round-trip lossily through double.

/// @brief Greatest common divisor (`g = gcd(a, b)`).
///
/// Elementwise. `gcd(0, 0) = 0`. `gcd(a, 0) = |a|`. Result is always
/// non-negative. Inputs broadcast.
///
/// @param a   First operand.
/// @param b   Second operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Elementwise GCD, broadcast shape.
/// @see lcm
Value gcd(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Least common multiple (`l = lcm(a, b)`).
///
/// Elementwise. `lcm(0, x) = 0`. Result is always non-negative.
///
/// @param a   First operand.
/// @param b   Second operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Elementwise LCM, broadcast shape.
/// @see gcd
Value lcm(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Bitwise AND over int64 reinterpretation (`y = bitand(a, b)`).
///
/// @param a   First operand.
/// @param b   Second operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Elementwise `a & b`, broadcast shape.
/// @see bitor_, bitxor_, bitcmp
Value bitand_(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Bitwise OR over int64 reinterpretation (`y = bitor(a, b)`).
///
/// @param a   First operand.
/// @param b   Second operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Elementwise `a | b`, broadcast shape.
/// @see bitand_, bitxor_
Value bitor_(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Bitwise XOR over int64 reinterpretation (`y = bitxor(a, b)`).
///
/// @param a   First operand.
/// @param b   Second operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Elementwise `a ^ b`, broadcast shape.
/// @see bitand_, bitor_
Value bitxor_(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Arithmetic bit shift (`y = bitshift(a, k)`).
///
/// Positive `k` → left shift; negative `k` → right shift
/// (arithmetic — sign-preserving for negative values).
///
/// @param a   Value(s) to shift.
/// @param k   Shift amount(s); broadcasts elementwise.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Shifted values, broadcast shape.
Value bitshift(const Value &a, const Value &k, std::pmr::memory_resource *mr = nullptr);

/// @brief Bitwise complement (`y = bitcmp(a, width)`).
///
/// Default `width = 64` (uint64 mask). Pass smaller `width`
/// (`8`, `16`, `32`, `64`) to restrict the complement to that many
/// low-order bits.
///
/// @param a      Value(s) to complement.
/// @param width  Bit width of the complement (default 64).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Bitwise complement, same shape as `a`.
/// @throws Error  Unsupported `width` (`m:bitcmp:badWidth`).
Value bitcmp(const Value &a, int width = 64, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
