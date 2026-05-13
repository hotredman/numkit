// libs/builtin/src/data_io/saveload.cpp
//
// Workspace-persistence builtins (save / load). Dispatches between the
// ascii backend (this file) and the matio v5 .mat backend
// (saveload_mat.cpp) based on flags and the filename's extension —
// matching MATLAB's defaults (binary .mat unless `-ascii` is given).

#include <numkit/io/workspace/saveload.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/environment.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace numkit::io {

// Implemented in saveload_mat.cpp when NUMKIT_WITH_MATIO is defined.
// When matio is disabled at build time, stubs below throw with a clear
// "binary .mat support disabled" message so `-mat` / `-v4` / `-v6` /
// `-v7` paths fail predictably instead of silently corrupting writes.
#ifdef NUMKIT_WITH_MATIO
void saveMat(Engine &engine, Environment &env,
             const std::string &filename,
             const std::vector<std::string> &varnames,
             int matVersion);
void loadMat(Engine &engine, Environment &env,
             const std::string &filename,
             size_t nargout, Span<Value> outs);
#else
static void saveMat(Engine &, Environment &,
                    const std::string &,
                    const std::vector<std::string> &,
                    int)
{
    throw Error("save: binary .mat support not compiled in "
                "(rebuild with NUMKIT_WITH_MATIO=ON)");
}
static void loadMat(Engine &, Environment &,
                    const std::string &,
                    size_t, Span<Value>)
{
    throw Error("load: binary .mat support not compiled in "
                "(rebuild with NUMKIT_WITH_MATIO=ON)");
}
#endif

namespace {

bool endsWithIgnoreCase(const std::string &s, const std::string &suffix)
{
    if (s.size() < suffix.size()) return false;
    for (size_t i = 0; i < suffix.size(); ++i) {
        char a = s[s.size() - suffix.size() + i];
        char b = suffix[i];
        if (std::tolower(static_cast<unsigned char>(a))
            != std::tolower(static_cast<unsigned char>(b)))
            return false;
    }
    return true;
}

} // namespace

// ════════════════════════════════════════════════════════════════════════
// save / load
//
//   save(filename, var1 [, var2 …] [, '-ascii' | '-mat' | '-v6' | '-v7'])
//   A = load(filename)
//   load(filename)   — without LHS:
//                       • ASCII: assigns to a var named after the file's
//                         stem (sans path + extension).
//                       • MAT:   assigns each stored variable into the
//                         caller's workspace under its stored name.
//
// Default format follows MATLAB: binary .mat (v5 via matio). `-ascii`
// forces the text backend. `-v7.3` (HDF5) is rejected — matio is built
// without the HDF5 backend in this project.
//
// ASCII scope: numeric (DOUBLE) matrices only; each row on its own
// line, columns space-separated, 17-significant-digit precision.
// Multiple vars in one save are blank-line separated.
//
// MAT scope: full v5 — numeric (real & complex), logical, char, cell,
// struct, struct arrays. See saveload_mat.cpp.
// ════════════════════════════════════════════════════════════════════════

void save(Engine &engine, Environment &env, Span<const Value> args)
{
    if (args.empty() || !args[0].isChar())
        throw Error("save: filename required");
    std::string filename = args[0].toString();

    // Three-way mode pick: explicit flag wins; otherwise extension hint;
    // otherwise MATLAB default (binary mat).
    enum class Mode { Auto, Ascii, Mat };
    Mode mode = Mode::Auto;
    // matVersion follows MATLAB-flag convention: 4 = MAT4, 5/6 = MAT5,
    // 7 = MAT5 + zlib. `-mat` alone (no version suffix) defaults to 5.
    // Whichever `-vN` appears last wins (mirrors MATLAB).
    int matVersion = 5;

    ScratchArena scratch(engine.resource());
    ScratchVec<std::string> varnames(&scratch);
    for (size_t i = 1; i < args.size(); ++i) {
        if (!args[i].isChar())
            continue;
        std::string s = args[i].toString();
        if (s == "-ascii") { mode = Mode::Ascii; continue; }
        if (s == "-mat") { mode = Mode::Mat; continue; }
        if (s == "-v4") { mode = Mode::Mat; matVersion = 4; continue; }
        if (s == "-v6") { mode = Mode::Mat; matVersion = 6; continue; }
        if (s == "-v7") { mode = Mode::Mat; matVersion = 7; continue; }
        if (s == "-v7.3")
            throw Error("save: -v7.3 (HDF5) is not supported in this build");
        if (s == "-append" || s == "-nocompression" || s == "-struct") {
            // accepted no-ops for now; -append is binary-only in MATLAB.
            if (s == "-append") mode = Mode::Mat;
            continue;
        }
        if (!s.empty() && s.front() == '-')
            throw Error("save: unsupported flag '" + s + "'");
        varnames.push_back(s);
    }

    if (mode == Mode::Auto)
        mode = endsWithIgnoreCase(filename, ".mat") ? Mode::Mat : Mode::Ascii;

    if (mode == Mode::Mat) {
        std::vector<std::string> names(varnames.begin(), varnames.end());
        saveMat(engine, env, filename, names, matVersion);
        return;
    }

    // ── ASCII backend ─────────────────────────────────────────────
    if (varnames.empty())
        throw Error("save: at least one variable name is required");

    std::ostringstream out;
    for (size_t vi = 0; vi < varnames.size(); ++vi) {
        Value *v = env.get(varnames[vi]);
        if (!v)
            throw Error("save: variable '" + varnames[vi] + "' not found");
        if (v->type() != ValueType::DOUBLE)
            throw Error("save: only numeric (double) variables supported in ascii mode");
        auto d = v->dims();
        size_t rows = d.rows();
        size_t cols = d.cols();
        if (vi > 0) out << "\n";
        if (rows == 0 || cols == 0) continue;
        for (size_t r = 0; r < rows; ++r) {
            for (size_t c = 0; c < cols; ++c) {
                if (c > 0) out << " ";
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.17g", (*v)(r, c));
                out << buf;
            }
            out << "\n";
        }
    }

    auto resolved = engine.resolvePath(filename);
    try {
        resolved.fs->writeFile(resolved.path, out.str());
    } catch (const std::exception &e) {
        throw Error(std::string("save: ") + e.what());
    }
}

void load(Engine &engine, Environment &env, Span<const Value> args,
          size_t nargout, Span<Value> outs)
{
    std::pmr::memory_resource *mr = engine.resource();
    if (args.empty() || !args[0].isChar())
        throw Error("load: filename required");
    std::string filename = args[0].toString();

    enum class Mode { Auto, Ascii, Mat };
    Mode mode = Mode::Auto;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!args[i].isChar()) continue;
        std::string s = args[i].toString();
        if (s == "-ascii") { mode = Mode::Ascii; continue; }
        if (s == "-mat" || s == "-v4" || s == "-v6" || s == "-v7") {
            mode = Mode::Mat; continue;
        }
        if (s == "-v7.3")
            throw Error("load: -v7.3 (HDF5) is not supported in this build");
        if (!s.empty() && s.front() == '-')
            throw Error("load: unsupported flag '" + s + "'");
    }

    if (mode == Mode::Auto)
        mode = endsWithIgnoreCase(filename, ".mat") ? Mode::Mat : Mode::Ascii;

    if (mode == Mode::Mat) {
        loadMat(engine, env, filename, nargout, outs);
        return;
    }

    // ── ASCII backend ─────────────────────────────────────────────
    auto resolved = engine.resolvePath(filename);
    std::string content;
    try {
        content = resolved.fs->readFile(resolved.path);
    } catch (const std::exception &e) {
        throw Error(std::string("load: ") + e.what());
    }

    // Parse each non-empty, non-comment line as whitespace-separated
    // doubles. MATLAB ignores '%' and '#' line comments.
    ScratchArena scratch(mr);
    ScratchVec<ScratchVec<double>> rows(&scratch);
    size_t p = 0;
    while (p <= content.size()) {
        size_t nl = content.find('\n', p);
        size_t end = (nl == std::string::npos) ? content.size() : nl;
        if (end == p && nl == std::string::npos) break;
        std::string line = content.substr(p, end - p);
        p = (nl == std::string::npos) ? content.size() + 1 : nl + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        size_t ws = line.find_first_not_of(" \t");
        if (ws == std::string::npos) continue;
        if (line[ws] == '%' || line[ws] == '#') continue;

        ScratchVec<double> row(&scratch);
        size_t q = ws;
        while (q < line.size()) {
            while (q < line.size() && std::isspace(static_cast<unsigned char>(line[q]))) ++q;
            if (q >= line.size()) break;
            const char *start = line.c_str() + q;
            char *endp = nullptr;
            double v = std::strtod(start, &endp);
            if (endp == start)
                throw Error("load: parse error near '" + line.substr(q) + "'");
            row.push_back(v);
            q = static_cast<size_t>(endp - line.c_str());
        }
        rows.push_back(std::move(row));
    }

    if (rows.empty())
        throw Error("load: no numeric data found");
    size_t cols = rows[0].size();
    for (auto &r : rows) {
        if (r.size() != cols)
            throw Error("load: inconsistent column count across rows");
    }
    size_t nrows = rows.size();

    Value M;
    if (nrows == 1 && cols == 1) {
        M = Value::scalar(rows[0][0], mr);
    } else {
        M = Value::matrix(nrows, cols, ValueType::DOUBLE, mr);
        double *data = M.doubleDataMut();
        for (size_t r = 0; r < nrows; ++r)
            for (size_t c = 0; c < cols; ++c)
                data[c * nrows + r] = rows[r][c];
    }

    if (nargout > 0) {
        outs[0] = std::move(M);
        return;
    }

    // No LHS — MATLAB assigns to a variable named after the file's
    // stem (basename without directory and extension).
    std::string stem = filename;
    size_t sep = stem.find_last_of("/\\:");
    if (sep != std::string::npos) stem = stem.substr(sep + 1);
    size_t dot = stem.find_last_of('.');
    if (dot != std::string::npos && dot > 0) stem = stem.substr(0, dot);
    if (stem.empty() || !(std::isalpha(static_cast<unsigned char>(stem[0])) || stem[0] == '_'))
        throw Error("load: cannot derive a valid variable name from filename");
    env.set(stem, std::move(M));
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void save_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    (void)nargout;
    (void)outs;
    save(*ctx.engine, *ctx.env, args);
}

void load_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    load(*ctx.engine, *ctx.env, args, nargout, outs);
}

} // namespace detail

} // namespace numkit::io
