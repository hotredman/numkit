// libs/builtin/include/numkit/m/builtin/datatypes/numeric/types.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

// ── Numeric type constructors (saturating for integers) ──────────────
/// double(x) — convert to DOUBLE (MATLAB: double). Renamed in C++ because
/// `double` is a keyword.
MValue toDouble(Allocator &alloc, const MValue &x);

/// logical(x) — convert to LOGICAL (non-zero → 1).
MValue logical(Allocator &alloc, const MValue &x);

/// single(x) — convert to SINGLE.
MValue single(Allocator &alloc, const MValue &x);

/// int8/int16/int32/int64(x) — saturating cast to signed integer.
MValue int8(Allocator &alloc, const MValue &x);
MValue int16(Allocator &alloc, const MValue &x);
MValue int32(Allocator &alloc, const MValue &x);
MValue int64(Allocator &alloc, const MValue &x);

/// uint8/uint16/uint32/uint64(x) — saturating cast to unsigned integer.
MValue uint8(Allocator &alloc, const MValue &x);
MValue uint16(Allocator &alloc, const MValue &x);
MValue uint32(Allocator &alloc, const MValue &x);
MValue uint64(Allocator &alloc, const MValue &x);

// ── Type predicates (logical scalar) ─────────────────────────────────
MValue isnumeric(Allocator &alloc, const MValue &x);
MValue islogical(Allocator &alloc, const MValue &x);
MValue ischar(Allocator &alloc, const MValue &x);
MValue isstring(Allocator &alloc, const MValue &x);
MValue iscell(Allocator &alloc, const MValue &x);
MValue isstruct(Allocator &alloc, const MValue &x);
MValue isempty(Allocator &alloc, const MValue &x);
MValue isscalar(Allocator &alloc, const MValue &x);
MValue isreal(Allocator &alloc, const MValue &x);
MValue isinteger(Allocator &alloc, const MValue &x);
MValue isfloat(Allocator &alloc, const MValue &x);
MValue issingle(Allocator &alloc, const MValue &x);

// ── Float-only predicates (elementwise) ──────────────────────────────
MValue isnan(Allocator &alloc, const MValue &x);
MValue isinf(Allocator &alloc, const MValue &x);
MValue isfinite(Allocator &alloc, const MValue &x);

// ── Equality ─────────────────────────────────────────────────────────
/// isequal(a, b) — deep equality, NaN != NaN.
MValue isequal(Allocator &alloc, const MValue &a, const MValue &b);

/// isequaln(a, b) — deep equality, NaN == NaN.
MValue isequaln(Allocator &alloc, const MValue &a, const MValue &b);

// ── Introspection ────────────────────────────────────────────────────
/// class(x) — MATLAB's class(). Returns char array with the type name
/// ("double", "single", "int32", ...). Renamed in C++ because `class` is
/// a keyword.
MValue classOf(Allocator &alloc, const MValue &x);

} // namespace numkit::m::builtin
