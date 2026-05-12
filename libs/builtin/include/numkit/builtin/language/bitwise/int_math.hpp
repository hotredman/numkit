// libs/builtin/include/numkit/builtin/language/bitwise/int_math.hpp
//
// Phase-7 integer-flavored numeric utilities: gcd / lcm / bitwise ops.
//
// For simplicity all inputs are read as doubles, converted to int64_t
// for the operation, and written back as doubles. This matches what
// MATLAB does for "small integer" workflows without forcing callers
// into explicit integer casts. Values outside the [-2^53, 2^53] range
// round-trip lossily through double — the same constraint MATLAB has.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// gcd(a, b) — greatest common divisor (element-wise). gcd(0,0) = 0.
/// gcd(a,0) = |a|. Result is always non-negative.
Value gcd(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// lcm(a, b) — least common multiple (element-wise). lcm(0, x) = 0.
/// Result is always non-negative.
Value lcm(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// bitand(a, b) — bitwise AND over int64 reinterpretation.
Value bitand_(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
Value bitor_ (const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
Value bitxor_(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// bitshift(a, k) — positive k = left shift, negative k = right shift
/// (arithmetic shift — sign-preserving for negative values).
Value bitshift(const Value &a, const Value &k, std::pmr::memory_resource *mr = nullptr);

/// bitcmp(a) — bitwise complement. Default width: 64 bits (uint64 mask).
/// Pass `width` to restrict to fewer bits (8, 16, 32, 64).
Value bitcmp(const Value &a, int width = 64, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
