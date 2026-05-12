// libs/builtin/include/numkit/builtin/language/types/types.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

// ── Numeric type constructors (saturating for integers) ──────────────
/// double(x) — convert to DOUBLE (MATLAB: double). Renamed in C++ because
/// `double` is a keyword.
Value toDouble(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// logical(x) — convert to LOGICAL (non-zero → 1).
Value logical(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// single(x) — convert to SINGLE.
Value single(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// int8/int16/int32/int64(x) — saturating cast to signed integer.
Value int8(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value int16(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value int32(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value int64(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// uint8/uint16/uint32/uint64(x) — saturating cast to unsigned integer.
Value uint8(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value uint16(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value uint32(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value uint64(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Type predicates (logical scalar) ─────────────────────────────────
Value isnumeric(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value islogical(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value ischar(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value isstring(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value iscell(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value isstruct(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value isempty(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value isscalar(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value isreal(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value isinteger(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value isfloat(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value issingle(const Value &x, std::pmr::memory_resource *mr = nullptr);
// numkit has no sparse-matrix storage class -- always returns false.
// Stub for parity with MATLAB scripts that probe sparse-ness.
Value issparse(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Float-only predicates (elementwise) ──────────────────────────────
Value isnan(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value isinf(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value isfinite(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Shape predicates ─────────────────────────────────────────────────
Value isvector(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value isrow(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value iscolumn(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value ismatrix(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Order predicates ─────────────────────────────────────────────────
/// issorted(A, mode?) — true if A is sorted under `mode` ("ascend",
/// "descend", "monotonic", "strictascend", "strictdescend"). For matrix
/// inputs, returns true iff every column is sorted under `mode`.
Value issorted(const Value &x, const Value *mode = nullptr, std::pmr::memory_resource *mr = nullptr);
/// issortedrows(A) — true if rows of A are in ascending lex order.
Value issortedrows(const Value &x, std::pmr::memory_resource *mr = nullptr);
/// isuniform(x) — true if vector x has constant first-differences (within
/// floating-point tolerance). For matrices, applies along the first
/// non-singleton dimension. Currently only the single-output form is
/// supported (no [tf, step] tuple).
Value isuniform(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Numeric limits / whole-array float predicates ────────────────────
/// flintmax(typeName?) — largest exact integer representable as the
/// given float type. Default 'double' → 2^53. 'single' → 2^24.
Value flintmax(const Value *typeName = nullptr, std::pmr::memory_resource *mr = nullptr);
/// intmax(typeName?) — largest value of the named integer class.
/// Default 'int32'. Returns a typed integer scalar.
Value intmax(const Value *typeName = nullptr, std::pmr::memory_resource *mr = nullptr);
/// intmin(typeName?) — smallest value of the named integer class.
Value intmin(const Value *typeName = nullptr, std::pmr::memory_resource *mr = nullptr);
/// realmax(typeName?) — largest finite value of the named float class.
Value realmax(const Value *typeName = nullptr, std::pmr::memory_resource *mr = nullptr);
/// realmin(typeName?) — smallest positive normal value of the named
/// float class.
Value realmin(const Value *typeName = nullptr, std::pmr::memory_resource *mr = nullptr);

/// allfinite(A) — equivalent to all(isfinite(A(:))) but single-pass.
Value allfinite(const Value &x, std::pmr::memory_resource *mr = nullptr);
/// anynan(A) — equivalent to any(isnan(A(:))).
Value anynan(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Equality ─────────────────────────────────────────────────────────
/// isequal(a, b) — deep equality, NaN != NaN.
Value isequal(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// isequaln(a, b) — deep equality, NaN == NaN.
Value isequaln(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

// ── Introspection ────────────────────────────────────────────────────
/// class(x) — MATLAB's class(). Returns char array with the type name
/// ("double", "single", "int32", ...). Renamed in C++ because `class` is
/// a keyword.
Value classOf(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Pack 36: cast + swapbytes ────────────────────────────────────────
/// cast(x, classname) — convert x to the type named by `classname`.
/// Accepted names: "double", "single", "int8".."int64", "uint8".."uint64",
/// "logical", "char", "string". Saturating for integers, same as the
/// per-type constructors (int32, etc.).
Value cast(const Value &x, const std::string &classname, std::pmr::memory_resource *mr = nullptr);

/// swapbytes(x) — reverse byte order within each element. Per MATLAB:
/// elements of size 1 (int8, uint8, logical, char) pass through.
/// double / single elements are bitwise-swapped, producing the same
/// result as memcpy → reverse → memcpy.
Value swapbytes(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// typecast(x, classname) — reinterpret the raw byte buffer of `x` as
/// elements of the named type. No value conversion: bit-for-bit
/// reinterpretation. The byte count must divide evenly by the new
/// element size. Output element count = byteCount / newElemSize.
/// Supported `classname`: same set as cast() except 'string' (string
/// arrays don't have a contiguous byte buffer).
Value typecast(const Value &x, const std::string &classname, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
