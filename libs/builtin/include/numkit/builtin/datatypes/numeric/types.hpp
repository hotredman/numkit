// libs/builtin/include/numkit/builtin/datatypes/numeric/types.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

// ── Numeric type constructors (saturating for integers) ──────────────
/// double(x) — convert to DOUBLE (MATLAB: double). Renamed in C++ because
/// `double` is a keyword.
Value toDouble(Allocator &alloc, const Value &x);

/// logical(x) — convert to LOGICAL (non-zero → 1).
Value logical(Allocator &alloc, const Value &x);

/// single(x) — convert to SINGLE.
Value single(Allocator &alloc, const Value &x);

/// int8/int16/int32/int64(x) — saturating cast to signed integer.
Value int8(Allocator &alloc, const Value &x);
Value int16(Allocator &alloc, const Value &x);
Value int32(Allocator &alloc, const Value &x);
Value int64(Allocator &alloc, const Value &x);

/// uint8/uint16/uint32/uint64(x) — saturating cast to unsigned integer.
Value uint8(Allocator &alloc, const Value &x);
Value uint16(Allocator &alloc, const Value &x);
Value uint32(Allocator &alloc, const Value &x);
Value uint64(Allocator &alloc, const Value &x);

// ── Type predicates (logical scalar) ─────────────────────────────────
Value isnumeric(Allocator &alloc, const Value &x);
Value islogical(Allocator &alloc, const Value &x);
Value ischar(Allocator &alloc, const Value &x);
Value isstring(Allocator &alloc, const Value &x);
Value iscell(Allocator &alloc, const Value &x);
Value isstruct(Allocator &alloc, const Value &x);
Value isempty(Allocator &alloc, const Value &x);
Value isscalar(Allocator &alloc, const Value &x);
Value isreal(Allocator &alloc, const Value &x);
Value isinteger(Allocator &alloc, const Value &x);
Value isfloat(Allocator &alloc, const Value &x);
Value issingle(Allocator &alloc, const Value &x);

// ── Float-only predicates (elementwise) ──────────────────────────────
Value isnan(Allocator &alloc, const Value &x);
Value isinf(Allocator &alloc, const Value &x);
Value isfinite(Allocator &alloc, const Value &x);

// ── Equality ─────────────────────────────────────────────────────────
/// isequal(a, b) — deep equality, NaN != NaN.
Value isequal(Allocator &alloc, const Value &a, const Value &b);

/// isequaln(a, b) — deep equality, NaN == NaN.
Value isequaln(Allocator &alloc, const Value &a, const Value &b);

// ── Introspection ────────────────────────────────────────────────────
/// class(x) — MATLAB's class(). Returns char array with the type name
/// ("double", "single", "int32", ...). Renamed in C++ because `class` is
/// a keyword.
Value classOf(Allocator &alloc, const Value &x);

} // namespace numkit::builtin
