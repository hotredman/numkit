// src/builtin/include/numkit/builtin/general.hpp
//
// Pure C++ General purpose commands, catalog introspection, help system, and session inspection.
#pragma once

#include <memory_resource>
#include <string>
#include <vector>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @file
/// @brief General utility functions, documentation query, and standard library catalog introspection.
///
/// Provides clean, engine-free C++ API for querying catalog help documentation,
/// listing category functions, and inspecting standard library built-ins.

// ── Catalog & Documentation Introspection ───────────────────────────────────

/// @brief Queries the standard library help system (`help(topic)`).
///
/// If @p query is empty, returns formatted text listing all available categories.
/// If @p query is a category name (e.g. `"elmat"`), returns documentation for that category.
/// If @p query is a function name (e.g. `"zeros"`), returns detailed function documentation and signature.
///
/// @param query Topic, category, or function name (empty for category catalog).
/// @return Formatted help text as a standard string.
/// @see what, builtins, categories
std::string help(const std::string &query = "");

/// @brief MATLAB-compatible overload for @ref help.
///
/// @param args Optional Span containing query string argument.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Character array Value containing formatted help documentation.
/// @see what, builtins
Value help(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Lists functions available in a specified toolbox category (`what(topic)`).
///
/// @param category Category or toolbox name (defaults to `"elmat"`).
/// @return Vector of function names in the category.
/// @see help, builtins, categories
std::vector<std::string> what(const std::string &category = "elmat");

/// @brief MATLAB-compatible overload for @ref what.
///
/// Returns a structure with fields `path`, `m` (cell array of function names),
/// `classes`, and `packages`.
///
/// @param args Optional Span containing topic name.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Structure Value matching MATLAB `what` output.
/// @see help, builtins
Value what(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Returns all built-in function names or functions in a specified category (`builtins(cat)`).
///
/// @param category Category name, or empty string to retrieve all registered standard library built-ins.
/// @return Vector of function names.
/// @see help, what, categories
std::vector<std::string> builtins(const std::string &category = "");

/// @brief MATLAB-compatible overload for @ref builtins.
///
/// @param args Optional Span containing category name.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Cell array Value of function name strings.
/// @see what, help
Value builtins(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Lists all standard library category names (`elmat`, `elfun`, `matfun`, etc.).
///
/// @return Vector of category names.
/// @see help, what, builtins
std::vector<std::string> categories();

/// @brief MATLAB-compatible overload for @ref categories.
///
/// @param mr Memory resource for allocations.
/// @return Cell array Value of category name strings.
/// @see builtins, help
Value categories(std::pmr::memory_resource *mr);

} // namespace numkit::builtin
