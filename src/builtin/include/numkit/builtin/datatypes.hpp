// src/builtin/include/numkit/builtin/datatypes.hpp
//
// Pure C++ Data types: cells, structures, type predicates, conversions, limits.
#pragma once

#include <memory_resource>
#include <string>
#include <vector>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @addtogroup group_datatypes
/// @{


/// @file
/// @ingroup group_datatypes
/// @brief Data type construction, inspection, structures, cell arrays, and limits.
///
/// Provides a clean, engine-free C++ API for type predicates, structure manipulation,
/// cell array queries, and numeric range limits.

// ── Structure Manipulation ──────────────────────────────────────────────────

/// @brief Tests if structure contains field by string name (`isfield(s, field)`).
/// @param s Structure value.
/// @param field Field name string.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Logical true if field exists.
/// @see getfield, setfield, rmfield
Value isfield(const Value &s, const std::string &field, std::pmr::memory_resource *mr = nullptr);

/// @brief Gets value of structure field by string name (`getfield(s, field)`).
/// @param s Structure value.
/// @param field Field name string.
/// @param mr Memory resource.
/// @return Value contained in structure field.
/// @see isfield, setfield, rmfield
Value getfield(const Value &s, const std::string &field, std::pmr::memory_resource *mr = nullptr);

/// @brief Sets value of structure field by string name (`setfield(s, field, val)`).
/// @param s Structure value.
/// @param field Field name string.
/// @param val New field value.
/// @param mr Memory resource.
/// @return Updated structure value.
/// @see getfield, isfield, rmfield
Value setfield(const Value &s, const std::string &field, const Value &val, std::pmr::memory_resource *mr = nullptr);

/// @brief Removes field from structure by string name (`rmfield(s, field)`).
/// @param s Structure value.
/// @param field Field name string to remove.
/// @param mr Memory resource.
/// @return Structure with field removed.
Value rmfield(const Value &s, const std::string &field, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input array to double-precision floating point (`double(x)`).
/// @param x Input array or scalar.
/// @param mr Memory resource.
/// @return Double-precision array.
Value toDouble(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input array to single-precision floating point (`single(x)`).
/// @param x Input array or scalar.
/// @param mr Memory resource.
/// @return Single-precision array.
Value single(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input array to 8-bit signed integer (`int8(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return 8-bit signed integer array.
Value int8(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input array to 16-bit signed integer (`int16(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return 16-bit signed integer array.
Value int16(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input array to 32-bit signed integer (`int32(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return 32-bit signed integer array.
Value int32(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input array to 64-bit signed integer (`int64(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return 64-bit signed integer array.
Value int64(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input array to 8-bit unsigned integer (`uint8(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return 8-bit unsigned integer array.
Value uint8(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input array to 16-bit unsigned integer (`uint16(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return 16-bit unsigned integer array.
Value uint16(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input array to 32-bit unsigned integer (`uint32(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return 32-bit unsigned integer array.
Value uint32(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input array to 64-bit unsigned integer (`uint64(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return 64-bit unsigned integer array.
Value uint64(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input array to logical boolean array (`logical(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Logical array where nonzero elements become true.
Value logical(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Type Predicates & Introspection ─────────────────────────────────────────

/// @brief Tests if input is a cell array (`iscell(v)`).
/// @param v Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see isstruct, isnumeric
Value iscell(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is a structure (`isstruct(v)`).
/// @param v Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see iscell, isfield
Value isstruct(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is numeric (double, single, integer) (`isnumeric(v)`).
/// @param v Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see isfloat, isinteger
Value isnumeric(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is a floating-point type (single or double) (`isfloat(v)`).
/// @param v Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see isnumeric, isinteger
Value isfloat(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is an integer class (`isinteger(v)`).
/// @param v Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see isfloat, isnumeric
Value isinteger(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is logical type (`islogical(v)`).
/// @param v Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
Value islogical(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is character array (`ischar(v)`).
/// @param v Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see isstring
Value ischar(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is string array (`isstring(v)`).
/// @param v Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see ischar
Value isstring(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests array elements for NaN (`isnan(v)`).
/// @param v Input array.
/// @param mr Memory resource.
/// @return Logical array matching shape of `v`.
/// @see isinf, isfinite
Value isnan(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests array elements for Infinity (`isinf(v)`).
/// @param v Input array.
/// @param mr Memory resource.
/// @return Logical array matching shape of `v`.
/// @see isnan, isfinite
Value isinf(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests array elements for finiteness (`isfinite(v)`).
/// @param v Input array.
/// @param mr Memory resource.
/// @return Logical array matching shape of `v`.
/// @see isnan, isinf
Value isfinite(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if array is empty (any dimension is 0) (`isempty(v)`).
/// @param v Input array.
/// @param mr Memory resource.
/// @return Logical scalar.
Value isempty(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is a 1x1 scalar (`isscalar(v)`).
/// @param v Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see isvector, ismatrix
Value isscalar(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is a 1-D vector (1xN or Nx1) (`isvector(v)`).
/// @param v Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see isrow, iscolumn, isscalar
Value isvector(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is a 1xN row vector (`isrow(v)`).
/// @param v Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see iscolumn, isvector
Value isrow(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is an Nx1 column vector (`iscolumn(v)`).
/// @param v Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see isrow, isvector
Value iscolumn(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is a 2-D matrix (`ismatrix(v)`).
/// @param v Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see isvector, isscalar
Value ismatrix(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is single-precision floating point (`issingle(x)`).
/// @param x Input array or value.
/// @param mr Memory resource.
/// @return Logical scalar.
Value issingle(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is a sparse array (`issparse(x)`).
/// @param x Input value.
/// @param mr Memory resource.
/// @return Logical scalar.
Value issparse(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Finds missing entries in array using default standard missing indicators (`ismissing(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Logical array where true indicates missing values (NaN, NaT, missing string/cell).
Value ismissing(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Finds missing entries in array matching user-specified indicators (`ismissing(x, indicator)`).
/// @param x Input array.
/// @param indicator Value or cell array of values treated as missing indicators.
/// @param mr Memory resource.
/// @return Logical array matching shape of `x`.
Value ismissing(const Value &x, const Value &indicator, std::pmr::memory_resource *mr = nullptr);

/// @brief Determines if array elements are sorted in ascending or descending order (`issorted(x, mode)`).
/// @param x Input array.
/// @param mode Sorting direction (e.g. `'ascend'`, `'descend'`).
/// @param mr Memory resource.
/// @return Logical scalar.
Value issorted(const Value &x, const Value &mode = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// @brief Determines if arrays are strictly identical in type, shape, and values (`isequal(a, b)`).
/// @param a First value.
/// @param b Second value.
/// @param mr Memory resource.
/// @return Logical scalar (NaNs are treated as not equal).
/// @see isequaln
Value isequal(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Determines if arrays are equal, treating NaNs as equal (`isequaln(a, b)`).
/// @param a First value.
/// @param b Second value.
/// @param mr Memory resource.
/// @return Logical scalar (NaNs in matching positions are treated as equal).
/// @see isequal
Value isequaln(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

// ── Type Conversion & Limits ────────────────────────────────────────────────

/// @brief Casts input array to target type name (`cast(v, targetType)`).
/// @param v Input array.
/// @param targetType Target class name (e.g. `'double'`, `'single'`, `'int32'`, `'uint8'`, `'logical'`).
/// @param mr Memory resource.
/// @return Casted array.
Value cast(const Value &v, const std::string &targetType, std::pmr::memory_resource *mr = nullptr);

/// @brief Replaces specified indicator values with standard missing values (`standardizeMissing(x, indicator)`).
/// @param x Input array.
/// @param indicator Value or array of values to convert into standard missing representation.
/// @param mr Memory resource.
/// @return Array with values replaced by NaNs / empty strings.
Value standardizeMissing(const Value &x, const Value &indicator, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if array contains any missing entries (`anymissing(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Logical true if any element is missing.
Value anymissing(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if array has uniform element spacing (`isuniform(x)`).
/// @param x Input vector.
/// @param mr Memory resource.
/// @return Logical scalar.
Value isuniform(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Returns the class name of an object or array (`class(x)`).
/// @param x Input value.
/// @param mr Memory resource.
/// @return String representing datatype class name.
Value classOf(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Swaps byte order of array elements between little-endian and big-endian (`swapbytes(x)`).
/// @param x Input integer or floating-point array.
/// @param mr Memory resource.
/// @return Array with reversed byte order per element.
Value swapbytes(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Reinterprets binary data between datatypes without changing underlying bytes (`typecast(x, datatype)`).
/// @param x Input numeric array.
/// @param datatype Target data type name.
/// @param mr Memory resource.
/// @return Typecasted array.
Value typecast(const Value &x, const std::string &datatype, std::pmr::memory_resource *mr = nullptr);

/// @brief True if all elements are finite numbers (`allfinite(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Logical scalar.
Value allfinite(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief True if any array element is NaN (`anynan(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Logical scalar.
Value anynan(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest positive normalized floating-point number (`realmin(className)`).
/// @param className Floating-point class name (`"double"` or `"single"`).
/// @param mr Memory resource.
/// @return Smallest positive normalized float.
/// @see realmax, eps
Value realmin(const std::string &className = "double", std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest positive normalized floating-point number (`realmin(t)`).
/// @param t Type name value.
/// @param mr Memory resource.
/// @return Smallest positive float.
Value realmin(const Value &t, std::pmr::memory_resource *mr = nullptr);

/// @brief Largest finite floating-point number (`realmax(className)`).
/// @param className Floating-point class name (`"double"` or `"single"`).
/// @param mr Memory resource.
/// @return Largest finite float.
/// @see realmin, intmax
Value realmax(const std::string &className = "double", std::pmr::memory_resource *mr = nullptr);

/// @brief Largest finite floating-point number (`realmax(t)`).
/// @param t Type name value.
/// @param mr Memory resource.
/// @return Largest finite float.
Value realmax(const Value &t, std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest integer value for specified integer class (`intmin(className)`).
/// @param className Integer class name (e.g. `"int8"`, `"int16"`, `"int32"`, `"int64"`, `"uint8"`, `"uint16"`, `"uint32"`, `"uint64"`).
/// @param mr Memory resource.
/// @return Smallest integer value.
/// @see intmax
Value intmin(const std::string &className = "int32", std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest integer value for specified integer class value (`intmin(t)`).
/// @param t Integer class value.
/// @param mr Memory resource.
/// @return Smallest integer value.
Value intmin(const Value &t, std::pmr::memory_resource *mr = nullptr);

/// @brief Largest integer value for specified integer class (`intmax(className)`).
/// @param className Integer class name.
/// @param mr Memory resource.
/// @return Largest integer value.
/// @see intmin
Value intmax(const std::string &className = "int32", std::pmr::memory_resource *mr = nullptr);

/// @brief Largest integer value for specified integer class value (`intmax(t)`).
/// @param t Integer class value.
/// @param mr Memory resource.
/// @return Largest integer value.
Value intmax(const Value &t, std::pmr::memory_resource *mr = nullptr);

/// @brief Largest consecutive integer accurately representable in floating-point format (`flintmax(className)`).
/// @param className Floating-point class name (`"double"` for `2^53`, `"single"` for `2^24`).
/// @param mr Memory resource.
/// @return Maximum consecutive float integer.
Value flintmax(const std::string &className = "double", std::pmr::memory_resource *mr = nullptr);

/// @brief Largest consecutive integer accurately representable in floating-point format (`flintmax(t)`).
/// @param t Type name value.
/// @param mr Memory resource.
/// @return Maximum consecutive float integer.
Value flintmax(const Value &t, std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::builtin
