// libs/io/include/numkit/io/paths/paths.hpp
//
// File-name construction utilities.

#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit { class Engine; }

namespace numkit::io {

using ::numkit::Engine;

/// @file
/// @brief Pure path-string manipulation + host temp-area access.
///
/// `filesep` / `fullfile` / `fileparts` are path-string ops; `tempdir`
/// and `tempname` reach into the host's temp area via the engine VFS.
/// All work on the host's native path conventions.

/// @brief Host path separator (`s = filesep()`).
///
/// Returns the single-character separator: `"\\"` on Windows, `"/"`
/// elsewhere.
///
/// @param mr  Memory resource (nullptr → process default).
/// @return    `1 × 1` CHAR.
/// @see fullfile
Value filesep(std::pmr::memory_resource *mr = nullptr);

/// @brief Concatenate path segments (`p = fullfile(parts)`).
///
/// Joins `parts` with @ref filesep. Trailing separators on individual
/// parts are normalised; absolute segments (after the first) are
/// appended literally with a separator, matching MATLAB behaviour.
///
/// @param parts  Path segments to join.
/// @param mr     Memory resource (nullptr → process default).
/// @return       CHAR path.
/// @see filesep, fileparts
Value fullfile(Span<const std::string> parts,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Split a path into folder / name / ext
/// (`[folder, name, ext] = fileparts(path)`).
///
/// `ext` includes the leading dot. `folder` has no trailing separator.
///
/// @param path  Input path string.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `(folder, name, ext)` triple of CHAR values.
/// @see fullfile
std::tuple<Value, Value, Value>
fileparts(const std::string &path,
          std::pmr::memory_resource *mr = nullptr);

/// @brief Host temp directory (`d = tempdir()`).
///
/// Includes a trailing separator. Routes through the engine VFS.
///
/// @param engine  Engine context (for VFS lookup).
/// @param mr      Memory resource (nullptr → process default).
/// @return        CHAR path.
/// @see tempname
Value tempdir(Engine &engine, std::pmr::memory_resource *mr = nullptr);

/// @brief Unique temp file path (`p = tempname()`).
///
/// Returns a path inside @ref tempdir. Does NOT create the file.
/// Uses a process-static counter combined with a random element so
/// consecutive calls don't collide.
///
/// @param engine  Engine context.
/// @param mr      Memory resource (nullptr → process default).
/// @return        CHAR path.
/// @see tempdir
Value tempname(Engine &engine, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::io
