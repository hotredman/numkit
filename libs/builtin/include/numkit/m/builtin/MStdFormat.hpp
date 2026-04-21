// libs/builtin/include/numkit/m/builtin/MStdFormat.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

#include <string>

namespace numkit::m::builtin {

/// Format a single printf-style invocation.
///
/// Does NOT cycle the format string over extra arguments — stops when the
/// format is exhausted. Supports MATLAB-style %d/%i/%u/%x/%X/%o/%f/%e/%E/
/// %g/%G/%s/%c/%% and backslash escapes (\n \t \\ \').
std::string formatOnce(const std::string &fmt, Span<const MValue> args,
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
/// Takes an Allocator because intermediate scalar MValues are allocated.
std::string formatCyclic(Allocator &alloc, const std::string &fmt,
                         Span<const MValue> args, size_t argStart);

/// MATLAB sprintf(fmt, args...) — char-array result. Empty fmt / non-char
/// fmt both return an empty char array (MATLAB behavior).
MValue sprintf(Allocator &alloc, const MValue &fmt, Span<const MValue> args);

} // namespace numkit::m::builtin
