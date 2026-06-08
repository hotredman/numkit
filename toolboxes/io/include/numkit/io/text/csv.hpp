// toolboxes/io/include/numkit/io/text/csv.hpp
#pragma once

#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

namespace numkit {
class Engine;
}

namespace numkit::io {

/// @file
/// @brief CSV text I/O.
///
/// Both routes go through the engine's VirtualFS (`resolvePath` +
/// `readFile` / `writeFile`), so `Engine &` is required. Signatures are
/// Span-based because `csvread` / `csvwrite` are inherently
/// variadic (range arg, offsets).

/// @brief `csvread` (`M = csvread(filename, R0, C0, range)`).
///
/// Reads a comma-delimited text file into a numeric matrix. Variadic
/// forms supported via `args`:
/// - `(filename)`                       — whole file from origin.
/// - `(filename, R0, C0)`               — start at row `R0`, col `C0`
///                                        (0-based).
/// - `(filename, R0, C0, [R1 C1 R2 C2])` — explicit range.
///
/// @param engine  Engine context (VFS).
/// @param args    Variadic arguments as documented above.
/// @return        DOUBLE matrix.
/// @throws Error  Bad args / file not found / parse failure.
/// @see csvwrite, readmatrix
Value csvread(Engine &engine, Span<const Value> args);

/// @brief `csvwrite` (`csvwrite(filename, M, R0, C0)`).
///
/// Writes a numeric matrix as comma-delimited text. Variadic forms:
/// - `(filename, M)`              — whole matrix at origin.
/// - `(filename, M, R0, C0)`      — start writing at `(R0, C0)`
///   (file is overwritten; offsets prepend blank columns / rows).
///
/// @param engine  Engine context (VFS).
/// @param args    Variadic arguments as documented above.
/// @throws Error  Bad args / write failure.
/// @see csvread, writematrix
void csvwrite(Engine &engine, Span<const Value> args);

} // namespace numkit::io
