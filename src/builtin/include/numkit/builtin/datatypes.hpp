// include/numkit/builtin/datatypes.hpp
//
// Data types: cells, structures, type predicates, conversions, limits.
#pragma once

#include <memory_resource>
#include <string>
#include <vector>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <numkit/runtime/language/cells/cell.hpp>
#include <numkit/runtime/language/structures/struct.hpp>

namespace numkit {
class Engine;
}

namespace numkit::builtin {

/// @file
/// @brief Data type construction, inspection, structures, cell arrays, and limits.

// ── Additional Structure Convenience Overloads ─────────────────────────────

/// @brief Tests if structure contains field by string name.
Value isfield(const Value &s, const std::string &field, std::pmr::memory_resource *mr = nullptr);

/// @brief Gets value of structure field by string name.
Value getfield(const Value &s, const std::string &field, std::pmr::memory_resource *mr = nullptr);

/// @brief Sets value of structure field by string name.
Value setfield(const Value &s, const std::string &field, const Value &val, std::pmr::memory_resource *mr = nullptr);

/// @brief Removes field from structure by string name.
Value rmfield(const Value &s, const std::string &field, std::pmr::memory_resource *mr = nullptr);

// ── Type Predicates & Introspection ─────────────────────────────────────────

/// @brief Tests if input is numeric (double, single, integer).
Value isnumeric(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is floating-point (double or single).
Value isfloat(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is integer type.
Value isinteger(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is logical type.
Value islogical(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is character array.
Value ischar(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is string array.
Value isstring(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests array elements for NaN.
Value isnan(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests array elements for Infinity.
Value isinf(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests array elements for finiteness.
Value isfinite(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if array is empty (any dimension is 0).
Value isempty(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is a 1x1 scalar.
Value isscalar(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is a 1-D vector (1xN or Nx1).
Value isvector(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is a 1xN row vector.
Value isrow(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is an Nx1 column vector.
Value iscolumn(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is a 2-D matrix.
Value ismatrix(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if two arrays are strictly equal.
Value isequal(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if two arrays are equal treating NaNs as equal.
Value isequaln(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

// ── Type Conversion & Limits ────────────────────────────────────────────────

/// @brief Casts input array to target type name ('double', 'single', 'int32', etc.).
Value cast(const Value &v, const std::string &targetType, std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest positive normalized floating-point number.
Value realmin(const std::string &className = "double", std::pmr::memory_resource *mr = nullptr);

/// @brief Largest positive normalized floating-point number.
Value realmax(const std::string &className = "double", std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest representable integer value.
Value intmin(const std::string &className = "int32", std::pmr::memory_resource *mr = nullptr);

/// @brief Largest representable integer value.
Value intmax(const std::string &className = "int32", std::pmr::memory_resource *mr = nullptr);

/// @brief Largest consecutive integer represented in floating-point.
Value flintmax(const std::string &className = "double", std::pmr::memory_resource *mr = nullptr);

// ── Registration ────────────────────────────────────────────────────────────

/// @brief Registers all datatype builtins into the engine instance.
void register_datatypes(Engine &engine);

} // namespace numkit::builtin
