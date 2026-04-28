// libs/builtin/src/MStdFileIO.cpp
//
// Simple file-I/O builtins (fopen/fclose/fgetl/fgets/feof/ferror/
// ftell/fseek/frewind). fread/fwrite + scan family live elsewhere
// (6c.8.4b / 6c.8.5) because they need the shared precision / size
// helpers.

#include <numkit/builtin/data_io/fileio.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "io_helpers.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Shared small helper
// ════════════════════════════════════════════════════════════════════════

namespace {

Engine::OpenFile *requireReadFid(Engine &engine, Span<const Value> args, const char *fn)
{
    if (args.empty() || !args[0].isScalar())
        throw Error(std::string(fn) + ": file identifier required");
    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f || !f->forRead)
        throw Error(std::string(fn) + ": invalid file identifier");
    return f;
}

} // namespace

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

void fopen(Engine &engine, Span<const Value> args, size_t nargout, Span<Value> outs)
{
    Allocator *alloc = &engine.allocator();
    if (args.empty() || !args[0].isChar())
        throw Error("fopen: filename must be a char array");

    // `fopen('all')` — only as the sole argument — returns a row vector
    // of every user-opened fid. With a mode arg, 'all' becomes a literal
    // filename instead.
    if (args.size() == 1 && args[0].toString() == "all") {
        auto ids = engine.openFileIds();
        if (ids.empty()) {
            outs[0] = Value::matrix(1, 0, ValueType::DOUBLE, alloc);
        } else {
            auto row = Value::matrix(1, ids.size(), ValueType::DOUBLE, alloc);
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
    outs[0] = Value::scalar(static_cast<double>(fid), alloc);
    // [fid, errmsg] = fopen(...) — errmsg is '' on success.
    if (nargout > 1)
        outs[1] = Value::fromString(fid < 0 ? engine.lastFopenError() : std::string(), alloc);
}

void fclose(Engine &engine, Span<const Value> args, size_t, Span<Value> outs)
{
    Allocator *alloc = &engine.allocator();
    if (args.empty())
        throw Error("fclose: requires a file identifier or 'all'");

    if (args[0].isChar() && args[0].toString() == "all") {
        engine.closeAllFiles();
        outs[0] = Value::scalar(0.0, alloc);
        return;
    }

    if (!args[0].isScalar())
        throw Error("fclose: argument must be a numeric fid or 'all'");
    int fid = static_cast<int>(args[0].toScalar());
    bool ok = engine.closeFile(fid);
    outs[0] = Value::scalar(ok ? 0.0 : -1.0, alloc);
}

void fgetl(Engine &engine, Span<const Value> args, size_t, Span<Value> outs)
{
    Allocator *alloc = &engine.allocator();
    auto *f = requireReadFid(engine, args, "fgetl");

    if (f->cursor >= f->buffer.size()) {
        f->lastError = "End of file reached.";
        outs[0] = Value::scalar(-1.0, alloc);
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
    outs[0] = Value::fromString(line, alloc);
}

void fgets(Engine &engine, Span<const Value> args, size_t, Span<Value> outs)
{
    Allocator *alloc = &engine.allocator();
    auto *f = requireReadFid(engine, args, "fgets");

    if (f->cursor >= f->buffer.size()) {
        f->lastError = "End of file reached.";
        outs[0] = Value::scalar(-1.0, alloc);
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
    outs[0] = Value::fromString(line, alloc);
}

void feof(Engine &engine, Span<const Value> args, size_t, Span<Value> outs)
{
    Allocator *alloc = &engine.allocator();
    if (args.empty() || !args[0].isScalar())
        throw Error("feof: file identifier required");
    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f)
        throw Error("feof: invalid file identifier");
    outs[0] = Value::logicalScalar(f->cursor >= f->buffer.size(), alloc);
}

void ferror(Engine &engine, Span<const Value> args, size_t nargout, Span<Value> outs)
{
    Allocator *alloc = &engine.allocator();
    if (args.empty() || !args[0].isScalar())
        throw Error("ferror: file identifier required");
    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f)
        throw Error("ferror: invalid file identifier");

    bool clear = args.size() >= 2 && args[1].isChar() && args[1].toString() == "clear";
    std::string msg = f->lastError;
    if (clear)
        f->lastError.clear();

    outs[0] = Value::fromString(msg, alloc);
    if (nargout > 1)
        outs[1] = Value::scalar(msg.empty() ? 0.0 : -1.0, alloc);
}

void ftell(Engine &engine, Span<const Value> args, size_t, Span<Value> outs)
{
    Allocator *alloc = &engine.allocator();
    if (args.empty() || !args[0].isScalar())
        throw Error("ftell: file identifier required");
    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f) {
        outs[0] = Value::scalar(-1.0, alloc);
        return;
    }
    // Write-mode: report end-of-buffer (where next append lands).
    size_t pos = f->forWrite ? f->buffer.size() : f->cursor;
    outs[0] = Value::scalar(static_cast<double>(pos), alloc);
}

void fseek(Engine &engine, Span<const Value> args, size_t, Span<Value> outs)
{
    Allocator *alloc = &engine.allocator();
    auto fail = [&]() { outs[0] = Value::scalar(-1.0, alloc); };

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
    outs[0] = Value::scalar(0.0, alloc);
}

void frewind(Engine &engine, Span<const Value> args, size_t, Span<Value>)
{
    if (args.empty() || !args[0].isScalar())
        throw Error("frewind: file identifier required");
    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f || !f->forRead)
        throw Error("frewind: invalid file identifier");
    f->cursor = 0;
}

// ── Binary I/O ──────────────────────────────────────────────────────────

void fread(Engine &engine, Span<const Value> args, size_t nargout, Span<Value> outs)
{
    Allocator *alloc = &engine.allocator();
    if (args.empty() || !args[0].isScalar())
        throw Error("fread: file identifier required");
    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f || !f->forRead)
        throw Error("fread: invalid file identifier");

    detail::SizeSpec sz{detail::SizeSpec::Kind::Flat, SIZE_MAX, 0, 0};
    if (args.size() >= 2)
        sz = detail::parseReadSize(args[1], "fread");

    std::string precStr = "uint8";
    if (args.size() >= 3) {
        if (!args[2].isChar())
            throw Error("fread: precision must be a char array");
        precStr = args[2].toString();
    }
    auto precOpt = detail::parsePrecision(precStr);
    if (!precOpt)
        throw Error("fread: unsupported precision '" + precStr + "'");
    int kind = precOpt->first;
    size_t bsize = precOpt->second;

    bool be = false;
    if (args.size() >= 4) {
        if (!args[3].isChar())
            throw Error("fread: machine format must be a char array");
        be = detail::parseEndian(args[3].toString(), "fread");
    }

    size_t available = f->buffer.size() - f->cursor;
    size_t maxElems = available / bsize;
    size_t n = std::min(sz.limit, maxElems);
    if (sz.limit != SIZE_MAX && n < sz.limit)
        f->lastError = "End of file reached.";

    std::vector<double> values(n);
    for (size_t i = 0; i < n; ++i) {
        // Local copy lets us byte-swap safely without mutating f->buffer.
        char tmp[8];
        std::memcpy(tmp, f->buffer.data() + f->cursor + i * bsize, bsize);
        if (be && bsize > 1) detail::byteSwap(tmp, bsize);

        double v = 0.0;
        if (kind == 0) {
            switch (bsize) {
            case 1: { uint8_t  x; std::memcpy(&x, tmp, 1); v = x; break; }
            case 2: { uint16_t x; std::memcpy(&x, tmp, 2); v = x; break; }
            case 4: { uint32_t x; std::memcpy(&x, tmp, 4); v = x; break; }
            case 8: { uint64_t x; std::memcpy(&x, tmp, 8); v = static_cast<double>(x); break; }
            }
        } else if (kind == 1) {
            switch (bsize) {
            case 1: { int8_t  x; std::memcpy(&x, tmp, 1); v = x; break; }
            case 2: { int16_t x; std::memcpy(&x, tmp, 2); v = x; break; }
            case 4: { int32_t x; std::memcpy(&x, tmp, 4); v = x; break; }
            case 8: { int64_t x; std::memcpy(&x, tmp, 8); v = static_cast<double>(x); break; }
            }
        } else {
            if (bsize == 4) { float x; std::memcpy(&x, tmp, 4); v = x; }
            else            {                     std::memcpy(&v, tmp, 8); }
        }
        values[i] = v;
    }
    f->cursor += n * bsize;
    outs[0] = detail::shapeFreadOutput(std::move(values), sz, alloc);
    if (nargout > 1)
        outs[1] = Value::scalar(static_cast<double>(n), alloc);
}

void fwrite(Engine &engine, Span<const Value> args, size_t, Span<Value> outs)
{
    Allocator *alloc = &engine.allocator();
    if (args.size() < 2 || !args[0].isScalar())
        throw Error("fwrite: requires (fid, array [, precision [, machineformat]])");
    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f || !f->forWrite)
        throw Error("fwrite: invalid file identifier");

    std::string precStr = "uint8";
    if (args.size() >= 3) {
        if (!args[2].isChar())
            throw Error("fwrite: precision must be a char array");
        precStr = args[2].toString();
    }
    auto precOpt = detail::parsePrecision(precStr);
    if (!precOpt)
        throw Error("fwrite: unsupported precision '" + precStr + "'");
    int kind = precOpt->first;
    size_t bsize = precOpt->second;

    bool be = false;
    if (args.size() >= 4) {
        if (!args[3].isChar())
            throw Error("fwrite: machine format must be a char array");
        be = detail::parseEndian(args[3].toString(), "fwrite");
    }

    const Value &A = args[1];
    size_t numel = A.numel();

    auto elemAsDouble = [&A](size_t i) -> double {
        if (A.type() == ValueType::DOUBLE) return A.doubleData()[i];
        if (A.isLogical())             return A.logicalData()[i] ? 1.0 : 0.0;
        throw Error("fwrite: unsupported array element type");
    };

    std::string bytes(numel * bsize, '\0');
    char *dst = bytes.data();
    for (size_t i = 0; i < numel; ++i) {
        double v = elemAsDouble(i);
        if (kind == 0) {
            switch (bsize) {
            case 1: { uint8_t  x = static_cast<uint8_t >(v); std::memcpy(dst, &x, 1); break; }
            case 2: { uint16_t x = static_cast<uint16_t>(v); std::memcpy(dst, &x, 2); break; }
            case 4: { uint32_t x = static_cast<uint32_t>(v); std::memcpy(dst, &x, 4); break; }
            case 8: { uint64_t x = static_cast<uint64_t>(v); std::memcpy(dst, &x, 8); break; }
            }
        } else if (kind == 1) {
            switch (bsize) {
            case 1: { int8_t  x = static_cast<int8_t >(v); std::memcpy(dst, &x, 1); break; }
            case 2: { int16_t x = static_cast<int16_t>(v); std::memcpy(dst, &x, 2); break; }
            case 4: { int32_t x = static_cast<int32_t>(v); std::memcpy(dst, &x, 4); break; }
            case 8: { int64_t x = static_cast<int64_t>(v); std::memcpy(dst, &x, 8); break; }
            }
        } else {
            if (bsize == 4) { float x = static_cast<float>(v); std::memcpy(dst, &x, 4); }
            else            {                                  std::memcpy(dst, &v, 8); }
        }
        if (be && bsize > 1)
            detail::byteSwap(dst, bsize);
        dst += bsize;
    }

    // Same cursor-based write contract as fprintf — appendOnly snaps to
    // the end regardless of any prior seek.
    size_t writePos = f->appendOnly ? f->buffer.size() : f->cursor;
    if (writePos + bytes.size() > f->buffer.size())
        f->buffer.resize(writePos + bytes.size());
    std::memcpy(f->buffer.data() + writePos, bytes.data(), bytes.size());
    f->cursor = writePos + bytes.size();
    outs[0] = Value::scalar(static_cast<double>(numel), alloc);
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

#define NK_FILEIO_REG(FN)                                                                          \
    void FN##_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)    \
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
NK_FILEIO_REG(fread)
NK_FILEIO_REG(fwrite)

#undef NK_FILEIO_REG

} // namespace detail

} // namespace numkit::builtin
