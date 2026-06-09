// toolboxes/io/src/text/extras.cpp
//
// fileread / readlines / writelines / readmatrix / writematrix / type.

#include <numkit/io/text/extras.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/fs/vfs.hpp>
#include <numkit/fs/fs_context.hpp>
#include <numkit/value/scratch.hpp>

#include <cmath>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace numkit::io {

namespace {

// Resolve via the engine and read the whole file as a string.
std::string slurpFile(FsContext &fs, const std::string &filename, const char *fnName)
{
    FsContext::ResolvedPath resolved{};
    try {
        resolved = fs.resolvePath(filename);
    } catch (const std::runtime_error &e) {
        throw Error(e.what());
    }
    if (!resolved.fs)
        throw Error(std::string(fnName) + ": cannot resolve filesystem for '" + filename + "'",
                     0, 0, fnName, "", std::string("numkit:") + fnName + ":noFs");
    try {
        return resolved.fs->readFile(resolved.path);
    } catch (const std::exception &e) {
        throw Error(std::string(fnName) + ": " + e.what(),
                     0, 0, fnName, "", std::string("numkit:") + fnName + ":readFailed");
    }
}

void spitFile(FsContext &fs, const std::string &filename, const std::string &content,
              const char *fnName)
{
    FsContext::ResolvedPath resolved{};
    try {
        resolved = fs.resolvePath(filename);
    } catch (const std::runtime_error &e) {
        throw Error(e.what());
    }
    if (!resolved.fs)
        throw Error(std::string(fnName) + ": cannot resolve filesystem for '" + filename + "'",
                     0, 0, fnName, "", std::string("numkit:") + fnName + ":noFs");
    try {
        resolved.fs->writeFile(resolved.path, content);
    } catch (const std::exception &e) {
        throw Error(std::string(fnName) + ": " + e.what(),
                     0, 0, fnName, "", std::string("numkit:") + fnName + ":writeFailed");
    }
}

Value charRow(std::pmr::memory_resource *mr, const std::string &s)
{
    auto v = Value::matrix(1, s.size(), ValueType::CHAR, mr);
    for (size_t i = 0; i < s.size(); ++i)
        v.charDataMut()[i] = s[i];
    return v;
}

// Split content by '\n', trimming '\r' from each line. Drop one trailing
// empty line that comes from a terminal newline.
std::vector<std::string> splitLines(const std::string &content)
{
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos <= content.size()) {
        size_t nl = content.find('\n', pos);
        size_t end = (nl == std::string::npos) ? content.size() : nl;
        std::string line = content.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(std::move(line));
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    // Strip trailing empty element produced by a terminal newline.
    if (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

bool tryParseDouble(const std::string &tok, double &out)
{
    if (tok.empty()) { out = 0.0; return true; }   // empty cell → 0
    try {
        size_t pos = 0;
        out = std::stod(tok, &pos);
        // Trailing junk → not a clean number.
        while (pos < tok.size() && std::isspace(static_cast<unsigned char>(tok[pos]))) ++pos;
        return pos == tok.size();
    } catch (...) {
        return false;
    }
}

// Comma / tab / semicolon delimited row.
std::vector<std::string> splitRow(const std::string &line)
{
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : line) {
        if (c == ',' || c == ';' || c == '\t') {
            tokens.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    tokens.push_back(cur);
    return tokens;
}

std::string trimWs(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

} // namespace

// ════════════════════════════════════════════════════════════════════════
// Engine-free PARSE / SERIALIZE cores (pure text ↔ Value — no Engine/VFS)
//
// Mirrors csvreadFromString / csvwriteToString: a C++ embedder can turn a
// delimited text blob into a numeric matrix (and back) with no Engine. The
// MATLAB-facing readmatrix / writematrix add VFS read/write on top.
// ════════════════════════════════════════════════════════════════════════

Value readmatrixFromString(const std::string &content, std::pmr::memory_resource *mr)
{
    auto lines = splitLines(content);

    // Skip leading non-numeric (header) rows: a row whose tokens are
    // all non-numeric (or which has any unparseable token) is considered
    // a header. Common case: header on row 1, numeric on row 2+.
    size_t skip = 0;
    for (; skip < lines.size(); ++skip) {
        if (trimWs(lines[skip]).empty()) continue;
        auto toks = splitRow(lines[skip]);
        bool anyNum = false;
        for (auto &t : toks) {
            double dummy;
            if (tryParseDouble(trimWs(t), dummy)) { anyNum = true; break; }
        }
        if (anyNum) break;
    }

    ScratchArena scratch(mr);
    ScratchVec<ScratchVec<double>> rows(&scratch);
    size_t maxCols = 0;
    for (size_t i = skip; i < lines.size(); ++i) {
        if (trimWs(lines[i]).empty()) continue;
        auto toks = splitRow(lines[i]);
        ScratchVec<double> row(&scratch);
        row.reserve(toks.size());
        for (auto &t : toks) {
            double v = 0.0;
            tryParseDouble(trimWs(t), v);     // unparseable → 0 (MATLAB compat)
            row.push_back(v);
        }
        if (row.size() > maxCols) maxCols = row.size();
        rows.push_back(std::move(row));
    }

    const size_t R = rows.size();
    auto out = Value::matrix(R, maxCols, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (size_t r = 0; r < R; ++r) {
        for (size_t c = 0; c < maxCols; ++c) {
            const double v = (c < rows[r].size()) ? rows[r][c] : 0.0;
            dst[c * R + r] = v;     // column-major
        }
    }
    return out;
}

std::string writematrixToString(const Value &m)
{
    if (m.isComplex())
        throw Error("writematrix: complex matrices are not supported",
                     0, 0, "writematrix", "", "numkit:writematrix:complex");
    const auto &d = m.dims();
    if (d.is3D())
        throw Error("writematrix: 3-D arrays are not supported",
                     0, 0, "writematrix", "", "numkit:writematrix:nd");
    const size_t R = d.rows(), C = d.cols();

    std::ostringstream os;
    os.precision(15);
    for (size_t r = 0; r < R; ++r) {
        for (size_t c = 0; c < C; ++c) {
            if (c) os << ',';
            const double v = m.elemAsDouble(c * R + r);
            if (std::isfinite(v) && std::floor(v) == v && std::abs(v) < 1e16) {
                os << static_cast<long long>(v);
            } else {
                os << v;
            }
        }
        os << '\n';
    }
    return os.str();
}

// ── fileread ──────────────────────────────────────────────────────────
Value fileread(FsContext &fs, const std::string &filename,
               std::pmr::memory_resource *mr)
{
    return charRow(mr, slurpFile(fs, filename, "fileread"));
}

// ── readlines ─────────────────────────────────────────────────────────
// Returns a STRING array (column vector), N×1.
Value readlines(FsContext &fs, const std::string &filename,
                std::pmr::memory_resource *mr)
{
    auto lines = splitLines(slurpFile(fs, filename, "readlines"));
    auto out = Value::stringArray(lines.size(), 1, mr);
    for (size_t i = 0; i < lines.size(); ++i)
        out.stringElemSet(i, lines[i]);
    return out;
}

// ── writelines ────────────────────────────────────────────────────────
void writelines(FsContext &fs, const Value &lines, const std::string &filename)
{
    std::ostringstream os;
    auto append = [&](const std::string &s) {
        os << s;
#ifdef _WIN32
        os << "\r\n";
#else
        os << '\n';
#endif
    };

    if (lines.isString() && lines.numel() != 1) {
        // String ARRAY → one line per element (MATLAB writelines contract).
        // Must precede the scalar branch: Value::toString() on a string array
        // returns only element [0], so the old (isChar||isString) catch-all
        // silently wrote just the first line. See bugs/io/writelines.md.
        const size_t n = lines.numel();
        for (size_t i = 0; i < n; ++i)
            append(lines.stringElem(i));
    } else if (lines.isChar() || lines.isString()) {
        // Single char row / scalar string → one line.
        append(lines.toString());
    } else if (lines.isCell()) {
        const size_t n = lines.numel();
        for (size_t i = 0; i < n; ++i) {
            const Value &cell = lines.cellAt(i);
            if (!cell.isChar() && !cell.isString())
                throw Error("writelines: each cell element must be a string",
                             0, 0, "writelines", "", "numkit:writelines:badCell");
            append(cell.toString());
        }
    } else {
        throw Error("writelines: lines must be a string, string array, or cell of strings",
                     0, 0, "writelines", "", "numkit:writelines:badArg");
    }
    spitFile(fs, filename, os.str(), "writelines");
}

// ── readmatrix ────────────────────────────────────────────────────────
Value readmatrix(FsContext &fs, const std::string &filename,
                 std::pmr::memory_resource *mr)
{
    return readmatrixFromString(slurpFile(fs, filename, "readmatrix"), mr);
}

// ── writematrix ───────────────────────────────────────────────────────
void writematrix(FsContext &fs, const Value &m, const std::string &filename)
{
    spitFile(fs, filename, writematrixToString(m), "writematrix");
}

// ── type ──────────────────────────────────────────────────────────────
void type(Engine &engine, const std::string &filename)
{
    auto content = slurpFile(engine.fsContext(), filename, "type");
    engine.outputText(content);
    if (content.empty() || content.back() != '\n')
        engine.outputText("\n");
}

namespace detail {

void fileread_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || (!args[0].isChar() && !args[0].isString()))
        throw Error("fileread: requires a filename string",
                     0, 0, "fileread", "", "numkit:fileread:nargin");
    outs[0] = fileread(ctx.engine->fsContext(), args[0].toString(), ctx.engine->resource());
}

void readlines_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || (!args[0].isChar() && !args[0].isString()))
        throw Error("readlines: requires a filename string",
                     0, 0, "readlines", "", "numkit:readlines:nargin");
    outs[0] = readlines(ctx.engine->fsContext(), args[0].toString(), ctx.engine->resource());
}

void writelines_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("writelines: requires (lines, filename)",
                     0, 0, "writelines", "", "numkit:writelines:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("writelines: filename must be a string",
                     0, 0, "writelines", "", "numkit:writelines:badFilename");
    writelines(ctx.engine->fsContext(), args[0], args[1].toString());
    outs[0] = Value();
}

void readmatrix_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || (!args[0].isChar() && !args[0].isString()))
        throw Error("readmatrix: requires a filename string",
                     0, 0, "readmatrix", "", "numkit:readmatrix:nargin");
    outs[0] = readmatrix(ctx.engine->fsContext(), args[0].toString(), ctx.engine->resource());
}

void writematrix_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("writematrix: requires (M, filename)",
                     0, 0, "writematrix", "", "numkit:writematrix:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("writematrix: filename must be a string",
                     0, 0, "writematrix", "", "numkit:writematrix:badFilename");
    writematrix(ctx.engine->fsContext(), args[0], args[1].toString());
    outs[0] = Value();
}

void type_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || (!args[0].isChar() && !args[0].isString()))
        throw Error("type: requires a filename string",
                     0, 0, "type", "", "numkit:type:nargin");
    type(*ctx.engine, args[0].toString());
    outs[0] = Value();
}

} // namespace detail

} // namespace numkit::io
