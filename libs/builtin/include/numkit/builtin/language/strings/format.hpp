// libs/builtin/include/numkit/builtin/language/strings/format.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit::builtin {

/// @brief Format a single printf-style invocation.
///
/// Does NOT cycle the format string over extra arguments — stops when
/// the format is exhausted. Supports MATLAB-style `%d` / `%i` / `%u` /
/// `%x` / `%X` / `%o` / `%f` / `%e` / `%E` / `%g` / `%G` / `%s` / `%c` /
/// `%%` and backslash escapes (`\n` `\t` `\\` `\'`).
///
/// @param fmt       printf-style format string.
/// @param args      Arguments referenced by `fmt`.
/// @param argStart  Index of the first arg to use (default 0).
/// @return          Formatted string.
std::string formatOnce(const std::string &fmt, Span<const Value> args,
                       size_t argStart = 0);

/// @brief Count `%`-format specifiers in `fmt`.
///
/// Excludes literal `%%`. Used by @ref formatCyclic to determine the
/// chunk size when cycling.
///
/// @param fmt  printf-style format string.
/// @return     Number of format specifiers in `fmt`.
size_t countFormatSpecs(const std::string &fmt);

/// @brief MATLAB-style cyclic format (`s = formatCyclic(fmt, args, start, mr)`).
///
/// Numeric arrays starting at `argStart` are flattened (column-major)
/// into a scalar stream; the format is re-applied to successive chunks
/// of `countFormatSpecs(fmt)` values. Char args pass through as single
/// tokens (`%s` consumes the whole string).
///
/// @param fmt       printf-style format string.
/// @param args      Arguments to format.
/// @param argStart  Index of the first arg to use.
/// @param mr        Memory resource for intermediate scalar Values
///                  (nullptr → process default).
/// @return          Formatted string.
std::string formatCyclic(const std::string &fmt, Span<const Value> args,
                         size_t argStart,
                         std::pmr::memory_resource *mr = nullptr);

/// @brief MATLAB `sprintf` (`s = sprintf(fmt, args...)`).
///
/// Returns a CHAR array. Empty `fmt` or non-char `fmt` both return an
/// empty char array (MATLAB behaviour).
///
/// @param fmt   Format string Value (CHAR / STRING).
/// @param args  Format arguments.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Formatted CHAR array.
Value sprintf(const Value &fmt, Span<const Value> args,
              std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
