// toolboxes/io/include/numkit/io/file_io/fileio.hpp
#pragma once

#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <memory_resource>

namespace numkit {
class FsContext;
}

namespace numkit::io {

/// @file
/// @brief File I/O builtins — thin C++ API over the FsContext file-handle table
/// (fopen/findFile).
///
/// Each function takes the FsContext (VFS + fid table; moved out of Engine)
/// and the argument span; results are written into
/// `outs`. `nargout` is forwarded because several of these populate
/// optional second returns (`[fid, errmsg]` from `fopen`,
/// `[msg, code]` from `ferror`, …).
///
/// The inherently variadic call surface makes a `Span`-based
/// signature the natural C++ API — typed wrappers would just re-parse
/// the same args.

/// @brief Open a file (`[fid, errmsg] = fopen(filename, permission, machineformat)`).
///
/// Resolves `filename` through the engine VFS, creates a new fid in
/// the engine's fid table, and returns it. On failure `fid = -1`
/// and `errmsg` is populated.
///
/// @param fs       FsContext (VFS + fid table).
/// @param args     Args: `(filename [, permission
///                 [, machineformat]])`.
/// @param nargout  Number of requested outputs (1 = fid only, 2 =
///                 `[fid, errmsg]`).
/// @param outs     Output slot(s).
/// @see fclose, fread, fwrite
void fopen(FsContext &fs, Span<const Value> args, size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr);

/// @brief Close a file (`status = fclose(fid)` or `fclose('all')`).
///
/// Releases the fid back to the engine's fid table.
///
/// @param fs       FsContext.
/// @param args     `(fid)` or `('all')`.
/// @param nargout  Number of requested outputs.
/// @param outs     Output slot for the status code.
/// @see fopen
void fclose(FsContext &fs, Span<const Value> args, size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr);

/// @brief Read one line, **without** the trailing newline
/// (`line = fgetl(fid)`).
///
/// @param fs       FsContext.
/// @param args     `(fid)`.
/// @param nargout  Number of requested outputs.
/// @param outs     Output slot for the line string (or -1 on EOF).
/// @see fgets, feof
void fgetl(FsContext &fs, Span<const Value> args, size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr);

/// @brief Read one line, **including** the trailing newline
/// (`line = fgets(fid [, nchar])`).
///
/// Optional second argument caps the number of characters read.
///
/// @param fs       FsContext.
/// @param args     `(fid [, nchar])`.
/// @param nargout  Number of requested outputs.
/// @param outs     Output slot for the line string (or -1 on EOF).
/// @see fgetl, feof
void fgets(FsContext &fs, Span<const Value> args, size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr);

/// @brief End-of-file test (`tf = feof(fid)`).
///
/// @param fs       FsContext.
/// @param args     `(fid)`.
/// @param nargout  Number of requested outputs.
/// @param outs     Output slot for the boolean status.
/// @see fgetl, fgets
void feof(FsContext &fs, Span<const Value> args, size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr);

/// @brief Last I/O error for a fid (`[msg, code] = ferror(fid [, 'clear'])`).
///
/// @param fs       FsContext.
/// @param args     `(fid [, 'clear'])`.
/// @param nargout  Number of requested outputs (1 = msg, 2 = `[msg, code]`).
/// @param outs     Output slots.
void ferror(FsContext &fs, Span<const Value> args, size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr);

/// @brief Current byte position in a file (`pos = ftell(fid)`).
///
/// @param fs       FsContext.
/// @param args     `(fid)`.
/// @param nargout  Number of requested outputs.
/// @param outs     Output slot for the byte offset.
/// @see fseek, frewind
void ftell(FsContext &fs, Span<const Value> args, size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr);

/// @brief Reposition a file pointer (`status = fseek(fid, offset, origin)`).
///
/// `origin` is `'bof'`, `'cof'`, `'eof'`, or the corresponding
/// numeric codes (-1, 0, 1).
///
/// @param fs       FsContext.
/// @param args     `(fid, offset, origin)`.
/// @param nargout  Number of requested outputs.
/// @param outs     Output slot for the status code.
/// @see ftell, frewind
void fseek(FsContext &fs, Span<const Value> args, size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr);

/// @brief Rewind a file pointer to the beginning (`frewind(fid)`).
///
/// Equivalent to `fseek(fid, 0, 'bof')`.
///
/// @param fs       FsContext.
/// @param args     `(fid)`.
/// @param nargout  Number of requested outputs (always 0).
/// @param outs     Unused output slot.
/// @see fseek
void frewind(FsContext &fs, Span<const Value> args, size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr);

// ── Binary I/O ───────────────────────────────────────────────────────

/// @brief Read raw binary data
/// (`A = fread(fid, size, precision, machineformat)`).
///
/// `size` may be `Inf` (read all remaining) or `[m n]` (matrix-shaped
/// output). `precision` selects the raw binary format;
/// `machineformat` picks big/little-endian.
///
/// @param fs       FsContext.
/// @param args     `(fid [, size [, precision [, machineformat]]])`.
/// @param nargout  Number of requested outputs.
/// @param outs     Output slot for the read array.
/// @see fwrite
void fread(FsContext &fs, Span<const Value> args, size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr);

/// @brief Write raw binary data
/// (`count = fwrite(fid, array, precision, machineformat)`).
///
/// Writes a `Value` as a packed binary stream into the fid's buffer.
///
/// @param fs       FsContext.
/// @param args     `(fid, array [, precision [, machineformat]])`.
/// @param nargout  Number of requested outputs.
/// @param outs     Output slot for the count of items written.
/// @see fread
void fwrite(FsContext &fs, Span<const Value> args, size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr);

} // namespace numkit::io
