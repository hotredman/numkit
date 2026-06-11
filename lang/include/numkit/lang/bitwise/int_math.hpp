// toolboxes/builtin/include/numkit/builtin/language/bitwise/int_math.hpp
//
// Phase-7 integer-flavored numeric utilities: gcd / lcm / bitwise ops.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::lang {

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

/// @brief Get bit `n` (1-based, LSB = 1) of each element (`b = bitget(a, n)`).
///
/// Reinterprets each element of `a` as int64 and returns its `n`-th bit
/// (0 or 1). Broadcasts `a` and `n` elementwise; bit positions outside
/// `1..64` yield 0.
///
/// @param a   Value(s) to read.
/// @param n   Bit position(s), 1-based (LSB = 1); broadcasts elementwise.
/// @param mr  Memory resource (nullptr → process default).
/// @return    The requested bits (0/1), broadcast shape.
/// @see bitset
Value bitget(const Value &a, const Value &n,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Set bit `n` (1-based, LSB = 1) of each element (`y = bitset(a, n, val)`).
///
/// Reinterprets each element of `a` as int64 and sets its `n`-th bit to
/// `val` (0 or 1). `val` defaults to 1 when omitted (`Value::Empty`).
/// Broadcasts `a` and `n` elementwise; bit positions outside `1..64`
/// leave the element unchanged.
///
/// @param a    Value(s) to modify.
/// @param n    Bit position(s), 1-based (LSB = 1); broadcasts elementwise.
/// @param val  Bit value 0 or 1 (default `Value::Empty` → 1).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Modified values, broadcast shape.
/// @throws Error if `val` is neither 0 nor 1.
/// @see bitget
Value bitset(const Value &a, const Value &n, const Value &val = Value::Empty,
             std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::lang
