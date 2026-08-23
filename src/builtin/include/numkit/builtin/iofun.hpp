// include/numkit/builtin/iofun.hpp
//
// Formatted input/output and stream printing functions.
#pragma once

#include <iostream>
#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit {
class Engine;
}

namespace numkit::builtin {

/// @file
/// @brief Formatted text I/O and display builtins.

/// @brief Formats data into a character string according to `fmt`.
Value sprintf(const Value &fmt, Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats data into a C++ string according to format string `fmt`.
std::string sprintf(const std::string &fmt, Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Displays value to the standard output or specified stream.
void disp(const Value &v, std::ostream &os = std::cout);

/// @brief Formats and writes data to an output stream.
int fprintf(std::ostream &os, const std::string &fmt, Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Reads formatted data from a string.
Value sscanf(const std::string &str, const std::string &fmt, std::pmr::memory_resource *mr = nullptr);

/// @brief Reads formatted data from a string into cell arrays.
Value textscan(const std::string &str, const std::string &fmt, std::pmr::memory_resource *mr = nullptr);

/// @brief Registers all I/O builtins into the engine instance.
void register_iofun(Engine &engine);

} // namespace numkit::builtin
