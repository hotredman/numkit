// libs/builtin/include/numkit/builtin/datatypes/numeric/types.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

// ── Numeric type constructors (saturating for integers) ──────────────
/// double(x) — convert to DOUBLE (MATLAB: double). Renamed in C++ because
/// `double` is a keyword.
Value toDouble(std::pmr::memory_resource *mr, const Value &x);

/// logical(x) — convert to LOGICAL (non-zero → 1).
Value logical(std::pmr::memory_resource *mr, const Value &x);

/// single(x) — convert to SINGLE.
Value single(std::pmr::memory_resource *mr, const Value &x);

/// int8/int16/int32/int64(x) — saturating cast to signed integer.
Value int8(std::pmr::memory_resource *mr, const Value &x);
Value int16(std::pmr::memory_resource *mr, const Value &x);
Value int32(std::pmr::memory_resource *mr, const Value &x);
Value int64(std::pmr::memory_resource *mr, const Value &x);

/// uint8/uint16/uint32/uint64(x) — saturating cast to unsigned integer.
Value uint8(std::pmr::memory_resource *mr, const Value &x);
Value uint16(std::pmr::memory_resource *mr, const Value &x);
Value uint32(std::pmr::memory_resource *mr, const Value &x);
Value uint64(std::pmr::memory_resource *mr, const Value &x);

// ── Type predicates (logical scalar) ─────────────────────────────────
Value isnumeric(std::pmr::memory_resource *mr, const Value &x);
Value islogical(std::pmr::memory_resource *mr, const Value &x);
Value ischar(std::pmr::memory_resource *mr, const Value &x);
Value isstring(std::pmr::memory_resource *mr, const Value &x);
Value iscell(std::pmr::memory_resource *mr, const Value &x);
Value isstruct(std::pmr::memory_resource *mr, const Value &x);
Value isempty(std::pmr::memory_resource *mr, const Value &x);
Value isscalar(std::pmr::memory_resource *mr, const Value &x);
Value isreal(std::pmr::memory_resource *mr, const Value &x);
Value isinteger(std::pmr::memory_resource *mr, const Value &x);
Value isfloat(std::pmr::memory_resource *mr, const Value &x);
Value issingle(std::pmr::memory_resource *mr, const Value &x);

// ── Float-only predicates (elementwise) ──────────────────────────────
Value isnan(std::pmr::memory_resource *mr, const Value &x);
Value isinf(std::pmr::memory_resource *mr, const Value &x);
Value isfinite(std::pmr::memory_resource *mr, const Value &x);

// ── Equality ─────────────────────────────────────────────────────────
/// isequal(a, b) — deep equality, NaN != NaN.
Value isequal(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// isequaln(a, b) — deep equality, NaN == NaN.
Value isequaln(std::pmr::memory_resource *mr, const Value &a, const Value &b);

// ── Introspection ────────────────────────────────────────────────────
/// class(x) — MATLAB's class(). Returns char array with the type name
/// ("double", "single", "int32", ...). Renamed in C++ because `class` is
/// a keyword.
Value classOf(std::pmr::memory_resource *mr, const Value &x);

} // namespace numkit::builtin
