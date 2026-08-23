// src/builtin/include/numkit/builtin/iofun.hpp
//
// Pure C++ Formatted input/output and stream printing functions.
#pragma once

#include <iostream>
#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @addtogroup group_iofun
/// @{


/// @file
/// @ingroup group_iofun
/// @brief Formatted text I/O and display builtins.
///
/// Provides clean, engine-free C++ API for formatted string generation (`sprintf`),
/// stream printing (`fprintf`, `disp`), and text parsing (`sscanf`, `textscan`).

/// @brief Formats data into a character array or string according to format specifier `fmt` (`sprintf(fmt, ...)`).
/// @param fmt Format specifier string Value.
/// @param args Span of arguments to format.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Formatted character array or string Value.
/// @see fprintf, sscanf
Value sprintf(const Value &fmt, Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats data into a C++ string according to format string `fmt`.
/// @param fmt Format string.
/// @param args Span of arguments to format.
/// @param mr Memory resource.
/// @return Formatted std::string.
/// @see fprintf, disp
std::string sprintf(const std::string &fmt, Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats Value for terminal display via `disp`.
/// @param a Input value to format.
/// @return Formatted string.
std::string dispFormat(const Value &a);

/// @brief Displays value to the standard output or specified stream (`disp(v)`).
/// @param v Value to display.
/// @param os Target output stream (defaults to std::cout).
void disp(const Value &v, std::ostream &os = std::cout);

/// @brief Formats and writes data to an output stream (`fprintf(os, fmt, ...)`).
/// @param os Target output stream.
/// @param fmt Format string.
/// @param args Span of arguments to format.
/// @param mr Memory resource.
/// @return Number of characters written.
/// @see sprintf, disp
int fprintf(std::ostream &os, const std::string &fmt, Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Reads formatted data from a string according to format specifier `fmt` (`sscanf(str, fmt)`).
/// @param str Source string to scan.
/// @param fmt Format specifier string.
/// @param mr Memory resource.
/// @return Scanned values array.
/// @see textscan, sprintf
Value sscanf(const std::string &str, const std::string &fmt, std::pmr::memory_resource *mr = nullptr);

/// @brief Reads formatted data from a string with multi-output support (`[A, count, errmsg, nextindex] = sscanf(...)`).
/// @param args Input arguments span `[str, fmt, size]`.
/// @param nargout Number of requested outputs.
/// @param outs Output spans to fill with results.
/// @param mr Memory resource.
void sscanf(Span<const Value> args, size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::builtin
