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

/// @file
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
/// @return Updated structure with field removed.
/// @see setfield, getfield, isfield
Value rmfield(const Value &s, const std::string &field, std::pmr::memory_resource *mr = nullptr);

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

/// @brief Tests if input is floating-point (double or single) (`isfloat(v)`).
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

/// @brief Tests if two arrays are strictly equal (`isequal(a, b)`).
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see isequaln
Value isequal(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if two arrays are equal treating NaNs as equal (`isequaln(a, b)`).
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Logical scalar.
/// @see isequal
Value isequaln(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

// ── Type Conversion & Limits ────────────────────────────────────────────────

/// @brief Casts input array to target type name (`cast(v, targetType)`).
/// @param v Input array.
/// @param targetType Target class name (e.g. `'double'`, `'single'`, `'int32'`, `'uint8'`, `'logical'`).
/// @param mr Memory resource.
/// @return Casted value array.
Value cast(const Value &v, const std::string &targetType, std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest positive normalized floating-point number (`realmin(class)`).
/// @param className Class name (`"double"` or `"single"`).
/// @param mr Memory resource.
/// @return Scalar minimum value.
/// @see realmax, eps
Value realmin(const std::string &className = "double", std::pmr::memory_resource *mr = nullptr);

/// @brief Largest positive normalized floating-point number (`realmax(class)`).
/// @param className Class name (`"double"` or `"single"`).
/// @param mr Memory resource.
/// @return Scalar maximum value.
/// @see realmin, intmax
Value realmax(const std::string &className = "double", std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest representable integer value (`intmin(class)`).
/// @param className Integer class name (`"int8"`, `"int16"`, `"int32"`, `"int64"`, `"uint8"`, etc.).
/// @param mr Memory resource.
/// @return Scalar minimum integer value.
/// @see intmax, realmin
Value intmin(const std::string &className = "int32", std::pmr::memory_resource *mr = nullptr);

/// @brief Largest representable integer value (`intmax(class)`).
/// @param className Integer class name (`"int8"`, `"int16"`, `"int32"`, `"int64"`, `"uint8"`, etc.).
/// @param mr Memory resource.
/// @return Scalar maximum integer value.
/// @see intmin, realmax
Value intmax(const std::string &className = "int32", std::pmr::memory_resource *mr = nullptr);

/// @brief Largest consecutive integer represented exactly in floating-point (`flintmax(class)`).
/// @param className Class name (`"double"` or `"single"`).
/// @param mr Memory resource.
/// @return Maximum consecutive flint integer (`2^53` for double, `2^24` for single).
/// @see eps, realmax
Value flintmax(const std::string &className = "double", std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
