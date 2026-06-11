// toolboxes/io/include/numkit/io/text/csv.hpp
#pragma once

#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <memory_resource>
#include <string>

namespace numkit {
class FsContext;
}

namespace numkit::io {

/// @brief Parse CSV text into a numeric DOUBLE matrix — **Engine-free** C++ API.
///
/// Pure text→Value: no Engine, no VFS (mirrors `image::imreadFromBytes`). The
/// MATLAB-facing `csvread` adds VFS path resolution + file read on top of this.
/// Missing / non-numeric cells read as 0.
Value csvreadFromString(const std::string &content,
                        std::pmr::memory_resource *mr = nullptr);

/// @brief As above, skipping the first `skipRows` rows / `skipCols` cols
/// (0-based origin, like `csvread(file, R0, C0)`). Engine-free.
Value csvreadFromString(const std::string &content, size_t skipRows, size_t skipCols,
                        std::pmr::memory_resource *mr = nullptr);

/// @brief Serialize a numeric matrix to CSV text — **Engine-free** C++ API.
/// `offR`/`offC` prepend blank rows/cols.
std::string csvwriteToString(const Value &M, size_t offR = 0, size_t offC = 0);

/// @file
/// @brief CSV text I/O.
///
/// Both routes go through the VirtualFS via `FsContext::resolvePath` +
/// `readFile` / `writeFile`, so a `FsContext &` is required (NOT an Engine —
/// path resolution is Engine-free; registration adapters pass
/// `engine.fsContext()`). Signatures are Span-based because `csvread` /
/// `csvwrite` are inherently variadic (range arg, offsets).

/// @brief `csvread` (`M = csvread(filename, R0, C0, range)`).
///
/// Reads a comma-delimited text file into a numeric matrix. Variadic
/// forms supported via `args`:
/// - `(filename)`                       — whole file from origin.
/// - `(filename, R0, C0)`               — start at row `R0`, col `C0`
///                                        (0-based).
/// - `(filename, R0, C0, [R1 C1 R2 C2])` — explicit range.
///
/// @param fs    Filesystem session (VFS registry + resolver).
/// @param args  Variadic arguments as documented above.
/// @param mr    Allocator for the result Value (defaults to default resource).
/// @return        DOUBLE matrix.
/// @throws Error  Bad args / file not found / parse failure.
/// @see csvwrite, readmatrix
Value csvread(FsContext &fs, Span<const Value> args,
              std::pmr::memory_resource *mr = nullptr);

/// @brief `csvwrite` (`csvwrite(filename, M, R0, C0)`).
///
/// Writes a numeric matrix as comma-delimited text. Variadic forms:
/// - `(filename, M)`              — whole matrix at origin.
/// - `(filename, M, R0, C0)`      — start writing at `(R0, C0)`
///   (file is overwritten; offsets prepend blank columns / rows).
///
/// @param fs    Filesystem session (VFS registry + resolver).
/// @param args  Variadic arguments as documented above.
/// @throws Error  Bad args / write failure.
/// @see csvread, writematrix
void csvwrite(FsContext &fs, Span<const Value> args);

} // namespace numkit::io
