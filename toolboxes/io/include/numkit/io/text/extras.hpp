// toolboxes/io/include/numkit/io/text/extras.hpp
//
// Modern text-file helpers.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>

namespace numkit { class Engine; class FsContext; }

namespace numkit::io {

using ::numkit::Engine;
using ::numkit::FsContext;

/// @file
/// @brief Modern text-file helpers.
///
/// File routes go through the VFS via `FsContext` so callers see consistent
/// path resolution (script origin, `NUMKIT_FS`, native fallback). `type`
/// alone keeps `Engine &` — it also needs the engine for output.

/// @brief Parse delimited text into a numeric DOUBLE matrix — **Engine-free**.
///
/// Pure text→Value (no Engine, no VFS), mirroring `csvreadFromString` /
/// `image::imreadFromBytes`. Comma / semicolon / tab delimited; a leading
/// all-non-numeric header row is skipped automatically; unparseable cells
/// read as 0. The MATLAB-facing `readmatrix` adds VFS read on top of this.
///
/// @param content  Delimited text.
/// @param mr       Memory resource (nullptr → process default).
/// @return         DOUBLE matrix.
Value readmatrixFromString(const std::string &content,
                           std::pmr::memory_resource *mr = nullptr);

/// @brief Serialize a numeric matrix to CSV text — **Engine-free**.
///
/// Comma-delimited; integer-valued doubles printed without decimals. Pure
/// Value→text (no Engine); `writematrix` adds VFS write on top.
///
/// @param m  Numeric (non-complex, ≤2-D) matrix.
/// @return   CSV text, one row per line, trailing newline.
/// @throws Error  Complex or 3-D input.
std::string writematrixToString(const Value &m);

/// @brief Read entire file into a CHAR row (`s = fileread(filename)`).
///
/// @param fs        Filesystem session (VFS registry + resolver).
/// @param filename  Path to file.
/// @param mr        Memory resource (nullptr → process default).
/// @return          `1 × N` CHAR row containing the file contents.
/// @throws Error    File not found / read error.
/// @see readlines, readmatrix
Value fileread(FsContext &fs, const std::string &filename,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Read file as STRING array (`L = readlines(filename)`).
///
/// One string per line. LF / CRLF normalised. Trailing empty line dropped.
///
/// @param fs        Filesystem session (VFS registry + resolver).
/// @param filename  Path to file.
/// @param mr        Memory resource (nullptr → process default).
/// @return          STRING column array.
/// @see fileread, writelines
Value readlines(FsContext &fs, const std::string &filename,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Write a STRING array as text lines (`writelines(lines, filename)`).
///
/// Each entry of `lines` becomes one line with a host-native ending.
/// File is overwritten if it exists.
///
/// @param fs        Filesystem session (VFS registry + resolver).
/// @param lines     STRING array or CELL of CHAR rows.
/// @param filename  Output path.
/// @throws Error    Write failure / unsupported `lines` type.
/// @see readlines
void writelines(FsContext &fs, const Value &lines, const std::string &filename);

/// @brief Read numeric matrix from text file (`M = readmatrix(filename)`).
///
/// Like `csvread` but skips a leading header row of non-numeric tokens
/// automatically.
///
/// @param fs        Filesystem session (VFS registry + resolver).
/// @param filename  Path to file.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Numeric matrix (DOUBLE).
/// @throws Error    File not found / parse failure.
/// @see writematrix, fileread
Value readmatrix(FsContext &fs, const std::string &filename,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Write numeric matrix as CSV (`writematrix(M, filename)`).
///
/// Default delimiter is comma; integer-valued doubles printed without
/// decimals.
///
/// @param fs        Filesystem session (VFS registry + resolver).
/// @param m         Numeric matrix to write.
/// @param filename  Output path.
/// @throws Error    Write failure / non-numeric input.
/// @see readmatrix
void writematrix(FsContext &fs, const Value &m, const std::string &filename);

/// @brief Print file content to the engine output (`type(filename)`).
///
/// Streams the file via `engine.output()`. No return value.
///
/// @param engine    Engine context.
/// @param filename  Path to file.
/// @throws Error    File not found.
void type(Engine &engine, const std::string &filename);

} // namespace numkit::io
