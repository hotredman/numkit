// libs/builtin/include/numkit/m/builtin/data_io/fileio.hpp
#pragma once

#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m {
class Engine;
}

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// File I/O builtins — thin C++ API over Engine::openFile / findFile.
//
// Each function takes the owning Engine (for the Vfs + fid table) and
// the MATLAB-style argument span; results are written into outs. nargout
// is forwarded because several of these populate optional second
// returns ([fid, errmsg] from fopen, [msg, code] from ferror, etc.).
//
// The inherently variadic MATLAB surface makes a Span-based signature
// the natural C++ API — cleaner shapes would just re-parse the same args.
// ════════════════════════════════════════════════════════════════════════

void fopen(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs);
void fclose(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs);
void fgetl(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs);
void fgets(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs);
void feof(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs);
void ferror(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs);
void ftell(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs);
void fseek(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs);
void frewind(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs);

// ── Binary I/O ───────────────────────────────────────────────────────
/// fread(fid, size, precision, machineformat) — reads a flat or matrix
/// block from the fid's buffer. `size` may be Inf (read all remaining)
/// or [m n] (matrix-shaped output). Precision selects the raw binary
/// format; machineformat picks big/little-endian.
void fread(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs);

/// fwrite(fid, array, precision, machineformat) — writes an MValue as
/// a packed binary stream into the fid's buffer.
void fwrite(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs);

} // namespace numkit::m::builtin
