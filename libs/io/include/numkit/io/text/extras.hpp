// libs/io/include/numkit/io/text/extras.hpp
//
// Modern text-file helpers (R2014b+ MATLAB):
//   fileread / readlines / writelines / readmatrix / writematrix / type.
// All go through the engine's VFS so callers see consistent path
// resolution (script origin, NUMKIT_FS, native fallback).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit { class Engine; }

namespace numkit::io {

using ::numkit::Engine;

/// fileread(filename) — read entire file into a 1×N char row vector.
Value fileread(std::pmr::memory_resource *mr, Engine &engine,
               const std::string &filename);

/// readlines(filename) — read file as a string array (one string per line).
/// LF / CRLF normalised. Trailing empty line dropped.
Value readlines(std::pmr::memory_resource *mr, Engine &engine,
                const std::string &filename);

/// writelines(lines, filename) — write a string / cell array of strings,
/// one per line, with a host-native line ending. The file is overwritten.
void writelines(Engine &engine, const Value &lines, const std::string &filename);

/// readmatrix(filename) — like csvread but skips a header row of
/// non-numeric tokens automatically. Returns a numeric matrix.
Value readmatrix(std::pmr::memory_resource *mr, Engine &engine,
                 const std::string &filename);

/// writematrix(M, filename) — write a numeric matrix as CSV. Default
/// delimiter is comma; integer-valued doubles printed without decimals.
void writematrix(Engine &engine, const Value &m, const std::string &filename);

/// type(filename) — print the file content via engine.output(). No
/// return value (returns empty Value).
void type(Engine &engine, const std::string &filename);

} // namespace numkit::io
