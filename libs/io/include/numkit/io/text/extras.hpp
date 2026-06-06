// libs/io/include/numkit/io/text/extras.hpp
//
// Modern text-file helpers.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>

namespace numkit { class Engine; }

namespace numkit::io {

using ::numkit::Engine;

/// @file
/// @brief Modern text-file helpers.
///
/// All routes go through the engine's VFS so callers see consistent
/// path resolution (script origin, `NUMKIT_FS`, native fallback).

/// @brief Read entire file into a CHAR row (`s = fileread(filename)`).
///
/// @param engine    Engine context (VFS).
/// @param filename  Path to file.
/// @param mr        Memory resource (nullptr → process default).
/// @return          `1 × N` CHAR row containing the file contents.
/// @throws Error    File not found / read error.
/// @see readlines, readmatrix
Value fileread(Engine &engine, const std::string &filename,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Read file as STRING array (`L = readlines(filename)`).
///
/// One string per line. LF / CRLF normalised. Trailing empty line dropped.
///
/// @param engine    Engine context.
/// @param filename  Path to file.
/// @param mr        Memory resource (nullptr → process default).
/// @return          STRING column array.
/// @see fileread, writelines
Value readlines(Engine &engine, const std::string &filename,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Write a STRING array as text lines (`writelines(lines, filename)`).
///
/// Each entry of `lines` becomes one line with a host-native ending.
/// File is overwritten if it exists.
///
/// @param engine    Engine context.
/// @param lines     STRING array or CELL of CHAR rows.
/// @param filename  Output path.
/// @throws Error    Write failure / unsupported `lines` type.
/// @see readlines
void writelines(Engine &engine, const Value &lines, const std::string &filename);

/// @brief Read numeric matrix from text file (`M = readmatrix(filename)`).
///
/// Like `csvread` but skips a leading header row of non-numeric tokens
/// automatically.
///
/// @param engine    Engine context.
/// @param filename  Path to file.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Numeric matrix (DOUBLE).
/// @throws Error    File not found / parse failure.
/// @see writematrix, fileread
Value readmatrix(Engine &engine, const std::string &filename,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Write numeric matrix as CSV (`writematrix(M, filename)`).
///
/// Default delimiter is comma; integer-valued doubles printed without
/// decimals.
///
/// @param engine    Engine context.
/// @param m         Numeric matrix to write.
/// @param filename  Output path.
/// @throws Error    Write failure / non-numeric input.
/// @see readmatrix
void writematrix(Engine &engine, const Value &m, const std::string &filename);

/// @brief Print file content to the engine output (`type(filename)`).
///
/// Streams the file via `engine.output()`. No return value.
///
/// @param engine    Engine context.
/// @param filename  Path to file.
/// @throws Error    File not found.
void type(Engine &engine, const std::string &filename);

} // namespace numkit::io
