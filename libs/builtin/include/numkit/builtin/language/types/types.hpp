// libs/builtin/include/numkit/builtin/language/types/types.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::builtin {

/// @file
/// @brief Type conversion, type predicates, and limits.
///
/// All predicate functions return LOGICAL scalars unless noted. All
/// numeric casts saturate at the target type's range. The C++ names
/// `toDouble` / `classOf` rename `double` / `class` (C++ keywords);
/// the registered names remain `double` / `class`.

// ── Numeric casts ────────────────────────────────────────────────────

/// @brief Convert to DOUBLE (`y = double(x)`).
///
/// C++ name `toDouble` because `double` is a reserved keyword.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE array, same shape as `x`.
/// @see single, logical, cast
Value toDouble(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Convert to LOGICAL (`y = logical(x)`).
///
/// Non-zero → 1, zero → 0.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL array, same shape as `x`.
/// @see toDouble, single
Value logical(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Convert to SINGLE (`y = single(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    SINGLE array, same shape as `x`.
/// @see toDouble, cast
Value single(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Saturating cast to INT8 (`y = int8(x)`).
/// @param x   Input array. @param mr  Memory resource.
/// @return    INT8 array, same shape as `x`. @see int16/int32/int64, cast
Value int8(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Saturating cast to INT16 (`y = int16(x)`).
/// @param x   Input array. @param mr  Memory resource. @return  INT16 array.
Value int16(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Saturating cast to INT32 (`y = int32(x)`).
/// @param x   Input array. @param mr  Memory resource. @return  INT32 array.
Value int32(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Saturating cast to INT64 (`y = int64(x)`).
/// @param x   Input array. @param mr  Memory resource. @return  INT64 array.
Value int64(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Saturating cast to UINT8 (`y = uint8(x)`).
/// @param x   Input array. @param mr  Memory resource. @return  UINT8 array.
/// @see uint16/uint32/uint64
Value uint8(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Saturating cast to UINT16 (`y = uint16(x)`).
/// @param x   Input array. @param mr  Memory resource. @return  UINT16 array.
Value uint16(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Saturating cast to UINT32 (`y = uint32(x)`).
/// @param x   Input array. @param mr  Memory resource. @return  UINT32 array.
Value uint32(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Saturating cast to UINT64 (`y = uint64(x)`).
/// @param x   Input array. @param mr  Memory resource. @return  UINT64 array.
Value uint64(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Type-class predicates ────────────────────────────────────────────

/// @brief True for any numeric type (`tf = isnumeric(x)`).
/// @param x   Input. @param mr  Memory resource.
/// @return    LOGICAL scalar. @see islogical, ischar, isfloat, isinteger
Value isnumeric(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True for LOGICAL arrays (`tf = islogical(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
Value islogical(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True for CHAR arrays (`tf = ischar(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see isstring
Value ischar(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True for STRING arrays (`tf = isstring(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see ischar
Value isstring(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True for CELL arrays (`tf = iscell(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
Value iscell(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True for STRUCT (`tf = isstruct(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
Value isstruct(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True for empty arrays (`tf = isempty(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see isscalar
Value isempty(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True for `1 × 1` arrays (`tf = isscalar(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see isempty, isvector
Value isscalar(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True if every element has zero imaginary part (`tf = isreal(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
Value isreal(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True for integer-typed arrays (`tf = isinteger(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see isfloat, isnumeric
Value isinteger(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True for DOUBLE or SINGLE (`tf = isfloat(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see isinteger
Value isfloat(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True for SINGLE arrays (`tf = issingle(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
Value issingle(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Stub for `issparse(x)` — numkit has no sparse storage class.
///
/// Always returns `false`. Provided for scripts that
/// probe sparseness.
///
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL false scalar.
Value issparse(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Numeric value predicates (elementwise) ───────────────────────────

/// @brief Elementwise NaN test (`tf = isnan(x)`).
/// @param x   Input. @param mr  Memory resource.
/// @return    LOGICAL array, same shape as `x`. @see isinf, isfinite, anynan
Value isnan(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise infinity test (`tf = isinf(x)`).
/// @param x   Input. @param mr  Memory resource.
/// @return    LOGICAL array, same shape as `x`. @see isfinite, isnan
Value isinf(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise finiteness test (`tf = isfinite(x)`).
/// @param x   Input. @param mr  Memory resource.
/// @return    LOGICAL array, same shape as `x`. @see isnan, isinf, allfinite
Value isfinite(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise missing-value test (`tf = ismissing(A[, indicator])`).
///
/// Without `indicator`: returns true where `A` contains the standard
/// missing value for its class. For DOUBLE / SINGLE this is `NaN`;
/// for integer / logical / character types nothing is treated as
/// missing (the result is all-false).
///
/// With `indicator` (scalar or vector): returns true where the
/// element matches any value in `indicator`. The element-wise
/// comparison uses `==` semantics; `NaN` in DOUBLE / SINGLE inputs
/// is still treated as missing (it matches if `indicator` contains
/// `NaN`, otherwise it is treated as missing only when no indicator
/// is provided).
///
/// @param x          Input array (any numeric or logical class).
/// @param indicator  Empty (default) for standard missing only, OR
///                   scalar / vector of values to also treat as missing.
/// @param mr         Memory resource (nullptr → process default).
/// @return           LOGICAL array of the same shape as `x`.
/// @see isnan, anymissing, rmmissing
Value ismissing(const Value &x, const Value &indicator,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Replace nonstandard missing-value indicators with NaN
/// (`B = standardizeMissing(A, indicator)`).
///
/// For DOUBLE / SINGLE inputs, every element matching any value in
/// `indicator` is replaced with NaN. For integer / logical inputs
/// the array passes through unchanged — those classes have no
/// standard missing value (matching MATLAB R2025b).
///
/// `NaN` in the indicator never matches (because `NaN != NaN` under
/// `==`); use `ismissing` instead if NaN match is needed.
///
/// @param x          Input array.
/// @param indicator  Scalar or vector of "nonstandard missing" values.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Array of the input class with replacements made.
/// @see ismissing, anymissing
Value standardizeMissing(const Value &x, const Value &indicator,
                         std::pmr::memory_resource *mr = nullptr);

/// @brief Scalar "any missing in array" check (`tf = anymissing(A)`).
///
/// True iff `ismissing(A)` is true for at least one element. Always
/// false for integer / logical / character / empty inputs.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL scalar.
/// @see ismissing, isnan
Value anymissing(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Shape predicates ─────────────────────────────────────────────────

/// @brief True for vectors (`tf = isvector(x)`).
///
/// `1 × n` or `n × 1` with `n >= 1`.
///
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see isrow, iscolumn, ismatrix
Value isvector(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True for row vectors (`tf = isrow(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see iscolumn, isvector
Value isrow(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True for column vectors (`tf = iscolumn(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see isrow, isvector
Value iscolumn(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True for 2-D arrays (`tf = ismatrix(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see isvector
Value ismatrix(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Sort predicates ──────────────────────────────────────────────────

/// @brief Test sortedness (`tf = issorted(A, mode)`).
///
/// Supported `mode` strings: `"ascend"` (default), `"descend"`,
/// `"monotonic"`, `"strictascend"`, `"strictdescend"`. For matrix
/// inputs returns `true` iff every column is sorted under `mode`.
///
/// @param x     Input array.
/// @param mode  Mode string (`Value::Empty` for default `"ascend"`).
/// @param mr    Memory resource.
/// @return      LOGICAL scalar.
/// @see issortedrows, isuniform
Value issorted(const Value &x, const Value &mode = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// @brief Test row-sortedness (`tf = issortedrows(A)`).
///
/// True iff rows of `A` are in ascending lexicographic order.
///
/// @param x   Input array. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see issorted
Value issortedrows(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Test uniform spacing (`tf = isuniform(x)`).
///
/// True if vector `x` has constant first-differences within FP tolerance.
/// For matrices applies along the first non-singleton dimension. Only
/// the single-output form is supported in this revision (no `[tf, step]`
/// tuple).
///
/// @param x   Input array. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see issorted
Value isuniform(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Limits ───────────────────────────────────────────────────────────

/// @brief Maximum exactly-representable integer (`y = flintmax(typeName)`).
///
/// `"double"` (default) → `2^53`; `"single"` → `2^24`.
///
/// @param typeName  Type-name string (`Value::Empty` → `"double"`).
/// @param mr        Memory resource.
/// @return          Scalar value of the requested floating-point class.
/// @see intmax, realmax
Value flintmax(const Value &typeName = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// @brief Largest value of an integer class (`y = intmax(typeName)`).
///
/// Default `"int32"`. Returns a typed integer scalar.
///
/// @param typeName  Integer-class string (`Value::Empty` → `"int32"`).
/// @param mr        Memory resource.
/// @return          Maximum value of the named integer class.
/// @see intmin, flintmax
Value intmax(const Value &typeName = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest value of an integer class (`y = intmin(typeName)`).
///
/// Default `"int32"`.
///
/// @param typeName  Integer-class string (`Value::Empty` → `"int32"`).
/// @param mr        Memory resource.
/// @return          Minimum value of the named integer class.
/// @see intmax
Value intmin(const Value &typeName = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// @brief Largest finite float (`y = realmax(typeName)`).
///
/// @param typeName  Float-class string (`"double"` default, `"single"`).
/// @param mr        Memory resource.
/// @return          Largest finite value of the named float class.
/// @see realmin, flintmax
Value realmax(const Value &typeName = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest positive normal float (`y = realmin(typeName)`).
///
/// @param typeName  Float-class string (`"double"` default).
/// @param mr        Memory resource.
/// @return          Smallest positive normal value.
/// @see realmax
Value realmin(const Value &typeName = Value::Empty, std::pmr::memory_resource *mr = nullptr);

// ── Reductions over finiteness ───────────────────────────────────────

/// @brief Single-pass `all(isfinite(A(:)))` (`tf = allfinite(A)`).
///
/// @param x   Input array. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see anynan, isfinite
Value allfinite(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Single-pass `any(isnan(A(:)))` (`tf = anynan(A)`).
///
/// @param x   Input array. @param mr  Memory resource. @return  LOGICAL scalar.
/// @see allfinite, isnan
Value anynan(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Deep equality ────────────────────────────────────────────────────

/// @brief Deep equality, NaN-aware (`tf = isequal(a, b)`).
///
/// Two values are equal iff they have the same type, shape, and
/// elementwise contents. `NaN != NaN` in this version (use @ref isequaln
/// for the NaN-tolerant variant).
///
/// @param a   First value.
/// @param b   Second value.
/// @param mr  Memory resource.
/// @return    LOGICAL scalar.
/// @see isequaln
Value isequal(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Deep equality treating NaN as equal (`tf = isequaln(a, b)`).
///
/// Like @ref isequal but `NaN == NaN`.
///
/// @param a   First value.
/// @param b   Second value.
/// @param mr  Memory resource.
/// @return    LOGICAL scalar.
/// @see isequal
Value isequaln(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

// ── Reflection / cast ────────────────────────────────────────────────

/// @brief Type name as a CHAR row (`s = class(x)`).
///
/// C++ name `classOf` because `class` is a keyword. Returns
/// `"double"`, `"single"`, `"int32"`, `"char"`, etc.
///
/// @param x   Input array.
/// @param mr  Memory resource.
/// @return    CHAR row containing the type name.
/// @see cast
Value classOf(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Cast to a named type (`y = cast(x, classname)`).
///
/// Accepted `classname`: `"double"`, `"single"`, `"int8"`..`"int64"`,
/// `"uint8"`..`"uint64"`, `"logical"`, `"char"`, `"string"`. Saturating
/// for integers (same as the per-type constructors).
///
/// @param x          Input array.
/// @param classname  Target type name.
/// @param mr         Memory resource.
/// @return           Array converted to the target type.
/// @throws Error  Unknown classname (`m:cast:badClass`).
/// @see classOf, typecast
Value cast(const Value &x, const std::string &classname, std::pmr::memory_resource *mr = nullptr);

/// @brief Reverse byte order within each element (`y = swapbytes(x)`).
///
/// Elements of size 1 (`int8`, `uint8`, `logical`, `char`) pass through.
/// `double` / `single` are bitwise-swapped — same as memcpy → reverse →
/// memcpy.
///
/// @param x   Input array.
/// @param mr  Memory resource.
/// @return    Byte-swapped array, same shape as `x`.
/// @see typecast
Value swapbytes(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Bit-for-bit reinterpretation (`y = typecast(x, classname)`).
///
/// No value conversion: the raw byte buffer of `x` is reinterpreted
/// as elements of the named type. Byte count must divide evenly by
/// the new element size. Output element count = `byteCount / newElemSize`.
/// Supported types: same set as @ref cast except `"string"` (string
/// arrays don't have a contiguous byte buffer).
///
/// @param x          Input array.
/// @param classname  Target type name.
/// @param mr         Memory resource.
/// @return           Reinterpreted array.
/// @throws Error  Byte count not divisible (`m:typecast:badLen`) or
///                unsupported class (`m:typecast:badClass`).
/// @see cast, swapbytes
Value typecast(const Value &x, const std::string &classname, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
