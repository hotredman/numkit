// libs/builtin/include/numkit/builtin/datatypes/numeric/int_math.hpp
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
Value gcd(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// lcm(a, b) — least common multiple (element-wise). lcm(0, x) = 0.
/// Result is always non-negative.
Value lcm(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// bitand(a, b) — bitwise AND over int64 reinterpretation.
Value bitand_(std::pmr::memory_resource *mr, const Value &a, const Value &b);
Value bitor_ (std::pmr::memory_resource *mr, const Value &a, const Value &b);
Value bitxor_(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// bitshift(a, k) — positive k = left shift, negative k = right shift
/// (arithmetic shift — sign-preserving for negative values).
Value bitshift(std::pmr::memory_resource *mr, const Value &a, const Value &k);

/// bitcmp(a) — bitwise complement. Default width: 64 bits (uint64 mask).
/// Pass `width` to restrict to fewer bits (8, 16, 32, 64).
Value bitcmp(std::pmr::memory_resource *mr, const Value &a, int width = 64);

} // namespace numkit::builtin
