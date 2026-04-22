// libs/builtin/include/numkit/m/builtin/MStdIntMath.hpp
//
// Phase-7 integer-flavored numeric utilities: gcd / lcm / bitwise ops.
//
// For simplicity all inputs are read as doubles, converted to int64_t
// for the operation, and written back as doubles. This matches what
// MATLAB does for "small integer" workflows without forcing callers
// into explicit integer casts. Values outside the [-2^53, 2^53] range
// round-trip lossily through double — the same constraint MATLAB has.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

/// gcd(a, b) — greatest common divisor (element-wise). gcd(0,0) = 0.
/// gcd(a,0) = |a|. Result is always non-negative.
MValue gcd(Allocator &alloc, const MValue &a, const MValue &b);

/// lcm(a, b) — least common multiple (element-wise). lcm(0, x) = 0.
/// Result is always non-negative.
MValue lcm(Allocator &alloc, const MValue &a, const MValue &b);

/// bitand(a, b) — bitwise AND over int64 reinterpretation.
MValue bitand_(Allocator &alloc, const MValue &a, const MValue &b);
MValue bitor_ (Allocator &alloc, const MValue &a, const MValue &b);
MValue bitxor_(Allocator &alloc, const MValue &a, const MValue &b);

/// bitshift(a, k) — positive k = left shift, negative k = right shift
/// (arithmetic shift — sign-preserving for negative values).
MValue bitshift(Allocator &alloc, const MValue &a, const MValue &k);

/// bitcmp(a) — bitwise complement. Default width: 64 bits (uint64 mask).
/// Pass `width` to restrict to fewer bits (8, 16, 32, 64).
MValue bitcmp(Allocator &alloc, const MValue &a, int width = 64);

} // namespace numkit::m::builtin
