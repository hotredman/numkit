// libs/builtin/src/MStdFileIO.cpp
//
// Simple file-I/O builtins (fopen/fclose/fgetl/fgets/feof/ferror/
// ftell/fseek/frewind). fread/fwrite + scan family live elsewhere
// (6c.8.4b / 6c.8.5) because they need the shared precision / size
// helpers.

#include <numkit/m/builtin/MStdFileIO.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include <algorithm>
#include <string>

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Shared small helper
// ════════════════════════════════════════════════════════════════════════

namespace {

Engine::OpenFile *requireReadFid(Engine &engine, Span<const MValue> args, const char *fn)
{
    if (args.empty() || !args[0].isScalar())
        throw MError(std::string(fn) + ": file identifier required");
    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f || !f->forRead)
        throw MError(std::string(fn) + ": invalid file identifier");
    return f;
}

} // namespace

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

void fopen(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs)
{
    Allocator *alloc = &engine.allocator();
    if (args.empty() || !args[0].isChar())
        throw MError("fopen: filename must be a char array");

    // `fopen('all')` — only as the sole argument — returns a row vector
    // of every user-opened fid. With a mode arg, 'all' becomes a literal
    // filename instead.
    if (args.size() == 1 && args[0].toString() == "all") {
        auto ids = engine.openFileIds();
        if (ids.empty()) {
            outs[0] = MValue::matrix(1, 0, MType::DOUBLE, alloc);
        } else {
            auto row = MValue::matrix(1, ids.size(), MType::DOUBLE, alloc);
            double *d = row.doubleDataMut();
            for (size_t i = 0; i < ids.size(); ++i)
                d[i] = static_cast<double>(ids[i]);
            outs[0] = std::move(row);
        }
        return;
    }

    std::string path = args[0].toString();
    std::string mode = (args.size() >= 2 && args[1].isChar()) ? args[1].toString() : "r";
    int fid = engine.openFile(path, mode);
    outs[0] = MValue::scalar(static_cast<double>(fid), alloc);
    // [fid, errmsg] = fopen(...) — errmsg is '' on success.
    if (nargout > 1)
        outs[1] = MValue::fromString(fid < 0 ? engine.lastFopenError() : std::string(), alloc);
}

void fclose(Engine &engine, Span<const MValue> args, size_t, Span<MValue> outs)
{
    Allocator *alloc = &engine.allocator();
    if (args.empty())
        throw MError("fclose: requires a file identifier or 'all'");

    if (args[0].isChar() && args[0].toString() == "all") {
        engine.closeAllFiles();
        outs[0] = MValue::scalar(0.0, alloc);
        return;
    }

    if (!args[0].isScalar())
        throw MError("fclose: argument must be a numeric fid or 'all'");
    int fid = static_cast<int>(args[0].toScalar());
    bool ok = engine.closeFile(fid);
    outs[0] = MValue::scalar(ok ? 0.0 : -1.0, alloc);
}

void fgetl(Engine &engine, Span<const MValue> args, size_t, Span<MValue> outs)
{
    Allocator *alloc = &engine.allocator();
    auto *f = requireReadFid(engine, args, "fgetl");

    if (f->cursor >= f->buffer.size()) {
        f->lastError = "End of file reached.";
        outs[0] = MValue::scalar(-1.0, alloc);
        return;
    }
    size_t start = f->cursor;
    size_t nl = f->buffer.find('\n', start);
    size_t end = (nl == std::string::npos) ? f->buffer.size() : nl;
    // Strip trailing \r so CRLF files don't leak the carriage return —
    // MATLAB does the same.
    size_t trimEnd = end;
    if (trimEnd > start && f->buffer[trimEnd - 1] == '\r')
        --trimEnd;
    std::string line = f->buffer.substr(start, trimEnd - start);
    f->cursor = (nl == std::string::npos) ? f->buffer.size() : nl + 1;
    outs[0] = MValue::fromString(line, alloc);
}

void fgets(Engine &engine, Span<const MValue> args, size_t, Span<MValue> outs)
{
    Allocator *alloc = &engine.allocator();
    auto *f = requireReadFid(engine, args, "fgets");

    if (f->cursor >= f->buffer.size()) {
        f->lastError = "End of file reached.";
        outs[0] = MValue::scalar(-1.0, alloc);
        return;
    }
    size_t start = f->cursor;
    size_t remaining = f->buffer.size() - start;
    size_t nchar = (args.size() >= 2 && args[1].isScalar())
                       ? static_cast<size_t>(args[1].toScalar())
                       : remaining;
    size_t nlPos = f->buffer.find('\n', start);
    size_t end;
    if (nlPos != std::string::npos && nlPos < start + nchar)
        end = nlPos + 1; // include newline
    else
        end = std::min(start + nchar, f->buffer.size());
    std::string line = f->buffer.substr(start, end - start);
    f->cursor = end;
    outs[0] = MValue::fromString(line, alloc);
}

void feof(Engine &engine, Span<const MValue> args, size_t, Span<MValue> outs)
{
    Allocator *alloc = &engine.allocator();
    if (args.empty() || !args[0].isScalar())
        throw MError("feof: file identifier required");
    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f)
        throw MError("feof: invalid file identifier");
    outs[0] = MValue::logicalScalar(f->cursor >= f->buffer.size(), alloc);
}

void ferror(Engine &engine, Span<const MValue> args, size_t nargout, Span<MValue> outs)
{
    Allocator *alloc = &engine.allocator();
    if (args.empty() || !args[0].isScalar())
        throw MError("ferror: file identifier required");
    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f)
        throw MError("ferror: invalid file identifier");

    bool clear = args.size() >= 2 && args[1].isChar() && args[1].toString() == "clear";
    std::string msg = f->lastError;
    if (clear)
        f->lastError.clear();

    outs[0] = MValue::fromString(msg, alloc);
    if (nargout > 1)
        outs[1] = MValue::scalar(msg.empty() ? 0.0 : -1.0, alloc);
}

void ftell(Engine &engine, Span<const MValue> args, size_t, Span<MValue> outs)
{
    Allocator *alloc = &engine.allocator();
    if (args.empty() || !args[0].isScalar())
        throw MError("ftell: file identifier required");
    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f) {
        outs[0] = MValue::scalar(-1.0, alloc);
        return;
    }
    // Write-mode: report end-of-buffer (where next append lands).
    size_t pos = f->forWrite ? f->buffer.size() : f->cursor;
    outs[0] = MValue::scalar(static_cast<double>(pos), alloc);
}

void fseek(Engine &engine, Span<const MValue> args, size_t, Span<MValue> outs)
{
    Allocator *alloc = &engine.allocator();
    auto fail = [&]() { outs[0] = MValue::scalar(-1.0, alloc); };

    if (args.size() < 2 || !args[0].isScalar() || !args[1].isScalar())
        return fail();

    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f || !f->forRead)
        return fail();

    long long offset = static_cast<long long>(args[1].toScalar());

    // Origin: defaults to 'bof' when omitted. Accepts either a string
    // ('bof'/'cof'/'eof') or the int codes -1/0/1 used by some
    // MATLAB-equivalent APIs.
    enum { BOF, COF, EOFO } origin = BOF;
    if (args.size() >= 3) {
        if (args[2].isChar()) {
            std::string o = args[2].toString();
            if (o == "bof") origin = BOF;
            else if (o == "cof") origin = COF;
            else if (o == "eof") origin = EOFO;
            else return fail();
        } else if (args[2].isScalar()) {
            int o = static_cast<int>(args[2].toScalar());
            if (o == -1) origin = BOF;
            else if (o == 0) origin = COF;
            else if (o == 1) origin = EOFO;
            else return fail();
        } else {
            return fail();
        }
    }

    long long base = 0;
    if (origin == BOF) base = 0;
    else if (origin == COF) base = static_cast<long long>(f->cursor);
    else base = static_cast<long long>(f->buffer.size());

    long long target = base + offset;
    if (target < 0 || static_cast<size_t>(target) > f->buffer.size())
        return fail();

    f->cursor = static_cast<size_t>(target);
    outs[0] = MValue::scalar(0.0, alloc);
}

void frewind(Engine &engine, Span<const MValue> args, size_t, Span<MValue>)
{
    if (args.empty() || !args[0].isScalar())
        throw MError("frewind: file identifier required");
    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f || !f->forRead)
        throw MError("frewind: invalid file identifier");
    f->cursor = 0;
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

#define NK_FILEIO_REG(FN)                                                                          \
    void FN##_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)    \
    {                                                                                              \
        FN(*ctx.engine, args, nargout, outs);                                                      \
    }

NK_FILEIO_REG(fopen)
NK_FILEIO_REG(fclose)
NK_FILEIO_REG(fgetl)
NK_FILEIO_REG(fgets)
NK_FILEIO_REG(feof)
NK_FILEIO_REG(ferror)
NK_FILEIO_REG(ftell)
NK_FILEIO_REG(fseek)
NK_FILEIO_REG(frewind)

#undef NK_FILEIO_REG

} // namespace detail

} // namespace numkit::m::builtin
