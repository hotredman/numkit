// libs/builtin/include/numkit/builtin/datatypes/strings/format.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit::builtin {

/// Format a single printf-style invocation.
///
/// Does NOT cycle the format string over extra arguments — stops when the
/// format is exhausted. Supports MATLAB-style %d/%i/%u/%x/%X/%o/%f/%e/%E/
/// %g/%G/%s/%c/%% and backslash escapes (\n \t \\ \').
std::string formatOnce(const std::string &fmt, Span<const Value> args,
                       size_t argStart = 0);

/// Count format specifiers (%d, %s, ...) in fmt, excluding literal %%.
size_t countFormatSpecs(const std::string &fmt);

/// Format with MATLAB's cyclic application over array inputs.
///
/// Numeric arrays starting at `argStart` are flattened (column-major) into
/// a scalar stream; the format is re-applied to successive chunks of
/// countFormatSpecs(fmt) values. Char args pass through as single tokens
/// (MATLAB %s consumes the whole string).
///
/// Takes a memory_resource because intermediate scalar MValues are allocated.
std::string formatCyclic(std::pmr::memory_resource *mr, const std::string &fmt,
                         Span<const Value> args, size_t argStart);

/// MATLAB sprintf(fmt, args...) — char-array result. Empty fmt / non-char
/// fmt both return an empty char array (MATLAB behavior).
Value sprintf(std::pmr::memory_resource *mr, const Value &fmt, Span<const Value> args);

} // namespace numkit::builtin
