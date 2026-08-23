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
Value rmfield(const Value &s, const std::string &field, std::pmr::memory_resource *mr = nullptr);

Value toDouble(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value single(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value int8(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value int16(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value int32(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value int64(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value uint8(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value uint16(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value uint32(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value uint64(const Value &x, std::pmr::memory_resource *mr = nullptr);
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

Value issingle(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value issparse(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value ismissing(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value ismissing(const Value &x, const Value &indicator, std::pmr::memory_resource *mr = nullptr);
Value issorted(const Value &x, const Value &mode = Value::Empty, std::pmr::memory_resource *mr = nullptr);

Value isequal(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
Value isequaln(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

// ── Type Conversion & Limits ────────────────────────────────────────────────

/// @brief Casts input array to target type name (`cast(v, targetType)`).
/// @param v Input array.
/// @param targetType Target class name (e.g. `'double'`, `'single'`, `'int32'`, `'uint8'`, `'logical'`).
Value cast(const Value &v, const std::string &targetType, std::pmr::memory_resource *mr = nullptr);

Value standardizeMissing(const Value &x, const Value &indicator, std::pmr::memory_resource *mr = nullptr);
Value anymissing(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value isuniform(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value classOf(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value swapbytes(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value typecast(const Value &x, const std::string &datatype, std::pmr::memory_resource *mr = nullptr);

Value allfinite(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value anynan(const Value &x, std::pmr::memory_resource *mr = nullptr);

Value realmin(const std::string &className = "double", std::pmr::memory_resource *mr = nullptr);
Value realmin(const Value &t, std::pmr::memory_resource *mr = nullptr);

Value realmax(const std::string &className = "double", std::pmr::memory_resource *mr = nullptr);
Value realmax(const Value &t, std::pmr::memory_resource *mr = nullptr);

Value intmin(const std::string &className = "int32", std::pmr::memory_resource *mr = nullptr);
Value intmin(const Value &t, std::pmr::memory_resource *mr = nullptr);

Value intmax(const std::string &className = "int32", std::pmr::memory_resource *mr = nullptr);
Value intmax(const Value &t, std::pmr::memory_resource *mr = nullptr);

Value flintmax(const std::string &className = "double", std::pmr::memory_resource *mr = nullptr);
Value flintmax(const Value &t, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
