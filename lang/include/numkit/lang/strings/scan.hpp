// toolboxes/builtin/include/numkit/builtin/language/strings/scan.hpp
#pragma once

#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

namespace numkit {
class Engine;
}

namespace numkit::lang {

/// @file
/// @brief Scan family — `fscanf` / `sscanf` / `textscan`.
///
/// Formatted reading. Like the other file-I/O
/// builtins, the natural C++ surface is a Span-based shape because
/// these functions are inherently variadic (optional size, optional
/// name/value option pairs). Reshaping into typed overloads would
/// only force every caller to re-parse the same dynamic argument
/// layout.
///
/// `fscanf` and `textscan` need the owning `Engine` because the fid
/// table lives there; `sscanf` is pure and takes just a
/// `memory_resource`.

/// @brief Formatted read from a file
/// (`[A, count] = fscanf(fid, format, sizeA)`).
///
/// @param engine   Engine context (fid table + VFS).
/// @param args     `(fid, format [, sizeA])`.
/// @param nargout  Number of requested outputs (1 = `A`, 2 = `[A, count]`).
/// @param outs     Output slot(s).
/// @see sscanf, textscan
void fscanf(Engine &engine, Span<const Value> args, size_t nargout, Span<Value> outs);  // lint: engine-io

/// @brief Formatted read from a string
/// (`[A, count] = sscanf(str, format, sizeA)`).
///
/// Pure function — takes only a memory resource, no engine state.
///
/// @param args     `(str, format [, sizeA])`.
/// @param nargout  Number of requested outputs.
/// @param outs     Output slot(s).
/// @param mr       Memory resource (nullptr → process default).
/// @see fscanf, textscan
void sscanf(Span<const Value> args, size_t nargout, Span<Value> outs,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Read formatted data from a file or string
/// (`C = textscan(fid_or_str, format [, name, value, …])`).
///
/// Cell-array-returning variant of the scan family with rich
/// name/value options (`'Delimiter'`, `'HeaderLines'`,
/// `'CollectOutput'`, …).
///
/// @param engine   Engine context (fid table + VFS).
/// @param args     `(fid_or_str, format [, options…])`.
/// @param nargout  Number of requested outputs.
/// @param outs     Output slot(s).
/// @see fscanf, sscanf
void textscan(Engine &engine, Span<const Value> args, size_t nargout, Span<Value> outs);  // lint: engine-io

} // namespace numkit::lang
