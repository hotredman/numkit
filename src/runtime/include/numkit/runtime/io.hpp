/// @file io.hpp
/// @ingroup group_iofun
// src/runtime/include/numkit/runtime/io.hpp
//
// Language-runtime I/O functions coupled to the Engine execution context
// (outputText sink, fid table, OpenFile).
#pragma once

#include <cstddef>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit {

/// @addtogroup group_iofun
/// @{

class Engine;
}

namespace numkit::runtime {

/// @brief Formats and writes text to engine stdout or open file descriptor.
/// @param engine Reference to executing Engine instance.
/// @param args Arguments [fid, fmt, ...] or [fmt, ...].
/// @return Number of characters written.
std::size_t fprintf(::numkit::Engine &engine, Span<const Value> args);

/// @brief Displays value to engine standard output (`disp(v)`).
/// @param engine Reference to executing Engine instance.
/// @param args Arguments span containing value to display.
void disp(::numkit::Engine &engine, Span<const Value> args);

/// @brief Reads formatted data from file descriptor (`fscanf(fid, fmt, size)`).
/// @param engine Reference to executing Engine instance.
/// @param args Input arguments span [fid, fmt, size].
/// @param nargout Number of requested outputs.
/// @param outs Output spans to fill with results.
void fscanf(::numkit::Engine &engine, Span<const Value> args, size_t nargout, Span<Value> outs);

/// @brief Reads tabular data from open file descriptor or string (`textscan(fid, fmt)`).
/// @param engine Reference to executing Engine instance.
/// @param args Input arguments span [fid/str, fmt, ...].
/// @param nargout Number of requested outputs.
/// @param outs Output spans to fill with results.
void textscan(::numkit::Engine &engine, Span<const Value> args, size_t nargout, Span<Value> outs);


/// @}
} // namespace numkit::runtime
