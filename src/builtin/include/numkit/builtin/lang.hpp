// src/builtin/include/numkit/builtin/lang.hpp
//
// Pure C++ Language keywords, variable name validation, environment, and evaluation.
#pragma once

#include <memory_resource>
#include <string>
#include <vector>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @file
/// @ingroup group_matlab
/// @brief Language keywords, variable name validation, process environment, and diagnostics.
///
/// Provides clean, engine-free C++ API for environment variable access,
/// keyword inspection, and variable name validation.

#include <numkit/fs/branding.hpp>

// ── Environment Variables ───────────────────────────────────────────────────

/// @brief Sets or clears an operating system environment variable (`setenv(name, value)`).
///
/// Sets the process environment variable @p name to @p value. If @p value is empty,
/// the variable is removed from the process environment.
///
/// @param name Environment variable name (must not be empty or contain '=').
/// @param value Target string value.
/// @throws std::runtime_error If @p name is empty or contains '='.
/// @see getenv
void setenv(const std::string &name, const std::string &value = "");

/// @brief Reads an operating system environment variable as a C++ string (`getenv(name)`).
///
/// @param name Environment variable name.
/// @return Variable value, or empty string `""` if unset.
/// @see setenv
std::string getenv(const std::string &name);

/// @brief Reads an environment variable returning a character row Value (`val = getenv(name)`).
///
/// @param name Variable name as Value (character array or string scalar).
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Character array Value containing the variable value, or empty `''` if unset.
/// @see setenv
Value getenv(const Value &name, std::pmr::memory_resource *mr = nullptr);

// ── Keyword & Identifier Inspection ─────────────────────────────────────────

/// @brief Tests whether a string is a reserved MATLAB language keyword (`iskeyword(s)`).
///
/// Reserved keywords: `break`, `case`, `catch`, `classdef`, `continue`, `else`,
/// `elseif`, `end`, `for`, `function`, `global`, `if`, `otherwise`, `parfor`,
/// `persistent`, `return`, `spmd`, `switch`, `try`, `while`.
///
/// @param name String identifier to test.
/// @return `true` if @p name is a reserved keyword, `false` otherwise.
/// @see isvarname, keywords
bool iskeyword(const std::string &name);

/// @brief Lists all reserved MATLAB keywords as a C++ vector.
///
/// @return Vector of keyword strings.
/// @see iskeyword
const std::vector<std::string> &keywords();

/// @brief MATLAB-compatible overload for @ref iskeyword.
///
/// If @p args is empty, returns a cell array of all keywords.
/// If @p args has 1 argument, tests whether that name is a keyword.
///
/// @param args Optional `(name)` argument.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL scalar if query provided, or cell array of strings if called with no arguments.
/// @see isvarname, keywords
Value iskeyword(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests whether a string is a valid MATLAB variable name (`isvarname(s)`).
///
/// A valid variable name begins with a letter, contains only alphanumeric characters
/// and underscores, and is not a reserved keyword.
///
/// @param name String to validate.
/// @return `true` if @p name is a valid variable name, `false` otherwise.
/// @see iskeyword
bool isvarname(const std::string &name);

/// @brief MATLAB-compatible overload for @ref isvarname.
///
/// Tests whether @p name (character array or string scalar) is a valid variable name.
/// Non-text inputs safely return `false`.
///
/// @param name Target Value to test.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return LOGICAL scalar `true` if valid, `false` otherwise.
/// @see iskeyword
Value isvarname(const Value &name, std::pmr::memory_resource *mr = nullptr);

/// @brief Span-based MATLAB-compatible overload for @ref isvarname.
///
/// @param args Span containing `(name)`.
/// @param mr Memory resource for allocations.
/// @return LOGICAL scalar Value.
/// @throws std::runtime_error If @p args is empty.
Value isvarname(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
