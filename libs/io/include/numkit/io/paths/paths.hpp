// libs/io/include/numkit/io/paths/paths.hpp
//
// File-name construction utilities. Pure path-string manipulation
// (filesep / fullfile / fileparts) and host temp-area access
// (tempdir / tempname). All work on the host's native path conventions.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit { class Engine; }

namespace numkit::io {

using ::numkit::Engine;

/// filesep() — single-character host path separator: "\\" on Windows,
/// "/" elsewhere. Returns a 1×1 char array.
Value filesep(std::pmr::memory_resource *mr);

/// fullfile(parts...) — concatenate path segments with filesep.
/// Trailing separators on individual parts are normalised; absolute
/// segments (after the first) are appended literally with a separator,
/// matching MATLAB behaviour.
Value fullfile(std::pmr::memory_resource *mr,
               const std::string *parts, size_t n);

/// fileparts(path) — split into (folder, name, ext). `ext` includes
/// the leading dot. `folder` has no trailing separator.
std::tuple<Value, Value, Value>
fileparts(std::pmr::memory_resource *mr, const std::string &path);

/// tempdir() — host temp directory, with trailing separator.
Value tempdir(std::pmr::memory_resource *mr, Engine &engine);

/// tempname() — unique temp file path inside tempdir(). Does not create
/// the file. Uses a process-static counter combined with a random
/// element so consecutive calls don't collide.
Value tempname(std::pmr::memory_resource *mr, Engine &engine);

} // namespace numkit::io
