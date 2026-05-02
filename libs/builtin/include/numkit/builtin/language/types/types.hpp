// libs/builtin/include/numkit/builtin/language/types/types.hpp
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

// ── Shape predicates ─────────────────────────────────────────────────
Value isvector(std::pmr::memory_resource *mr, const Value &x);
Value isrow(std::pmr::memory_resource *mr, const Value &x);
Value iscolumn(std::pmr::memory_resource *mr, const Value &x);
Value ismatrix(std::pmr::memory_resource *mr, const Value &x);

// ── Order predicates ─────────────────────────────────────────────────
/// issorted(A, mode?) — true if A is sorted under `mode` ("ascend",
/// "descend", "monotonic", "strictascend", "strictdescend"). For matrix
/// inputs, returns true iff every column is sorted under `mode`.
Value issorted(std::pmr::memory_resource *mr, const Value &x,
               const Value *mode = nullptr);
/// issortedrows(A) — true if rows of A are in ascending lex order.
Value issortedrows(std::pmr::memory_resource *mr, const Value &x);
/// isuniform(x) — true if vector x has constant first-differences (within
/// floating-point tolerance). For matrices, applies along the first
/// non-singleton dimension. Currently only the single-output form is
/// supported (no [tf, step] tuple).
Value isuniform(std::pmr::memory_resource *mr, const Value &x);

// ── Numeric limits / whole-array float predicates ────────────────────
/// flintmax(typeName?) — largest exact integer representable as the
/// given float type. Default 'double' → 2^53. 'single' → 2^24.
Value flintmax(std::pmr::memory_resource *mr, const Value *typeName = nullptr);
/// intmax(typeName?) — largest value of the named integer class.
/// Default 'int32'. Returns a typed integer scalar.
Value intmax(std::pmr::memory_resource *mr, const Value *typeName = nullptr);
/// intmin(typeName?) — smallest value of the named integer class.
Value intmin(std::pmr::memory_resource *mr, const Value *typeName = nullptr);
/// realmax(typeName?) — largest finite value of the named float class.
Value realmax(std::pmr::memory_resource *mr, const Value *typeName = nullptr);
/// realmin(typeName?) — smallest positive normal value of the named
/// float class.
Value realmin(std::pmr::memory_resource *mr, const Value *typeName = nullptr);

/// allfinite(A) — equivalent to all(isfinite(A(:))) but single-pass.
Value allfinite(std::pmr::memory_resource *mr, const Value &x);
/// anynan(A) — equivalent to any(isnan(A(:))).
Value anynan(std::pmr::memory_resource *mr, const Value &x);

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

// ── Pack 36: cast + swapbytes ────────────────────────────────────────
/// cast(x, classname) — convert x to the type named by `classname`.
/// Accepted names: "double", "single", "int8".."int64", "uint8".."uint64",
/// "logical", "char", "string". Saturating for integers, same as the
/// per-type constructors (int32, etc.).
Value cast(std::pmr::memory_resource *mr, const Value &x,
           const std::string &classname);

/// swapbytes(x) — reverse byte order within each element. Per MATLAB:
/// elements of size 1 (int8, uint8, logical, char) pass through.
/// double / single elements are bitwise-swapped, producing the same
/// result as memcpy → reverse → memcpy.
Value swapbytes(std::pmr::memory_resource *mr, const Value &x);

} // namespace numkit::builtin
