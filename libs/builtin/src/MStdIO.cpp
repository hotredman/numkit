#include <numkit/m/core/MBranding.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

namespace numkit::m {

void StdLibrary::registerIOFunctions(Engine &engine)
{
    // disp / fprintf now live in libs/builtin/src/MStdPrint.cpp.
    // sprintf / formatOnce / formatCyclic / countFormatSpecs live in
    // libs/builtin/src/MStdFormat.cpp.
    // error / warning / MException / rethrow / throw / assert live in
    // libs/builtin/src/MStdDiagnostics.cpp.

    // ── csvread / csvwrite ──────────────────────────────────────
    //
    // csvread(filename)                      → read numeric CSV into a matrix
    // csvread(filename, R1, C1)              → skip first R1 rows / C1 cols (0-based)
    // csvread(filename, R1, C1, [R1 C1 R2 C2]) → read only the rectangular range
    //
    // csvwrite(filename, M)                  → write M as CSV
    // csvwrite(filename, M, R, C)            → write M offset by R rows, C cols
    //
    // Missing cells are read as 0. Filenames with no extension get ".csv".

    auto resolveCsvPath = [](std::string path) -> std::string {
        size_t slash = path.find_last_of("/\\");
        size_t fromIdx = (slash == std::string::npos) ? 0 : slash + 1;
        if (path.find('.', fromIdx) == std::string::npos)
            path += ".csv";
        return path;
    };

    auto parseCsvLine = [](const std::string &raw) -> std::vector<double> {
        std::vector<double> out;
        size_t len = raw.size();
        if (len && raw[len - 1] == '\r')
            --len;

        size_t start = 0;
        for (size_t i = 0; i <= len; ++i) {
            if (i == len || raw[i] == ',') {
                const char *p = raw.c_str() + start;
                size_t n = i - start;
                while (n > 0 && (*p == ' ' || *p == '\t')) {
                    ++p;
                    --n;
                }
                while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t'))
                    --n;
                double v = 0.0;
                if (n > 0) {
                    std::string cell(p, n);
                    char *endp = nullptr;
                    double parsed = std::strtod(cell.c_str(), &endp);
                    if (endp != cell.c_str())
                        v = parsed;
                }
                out.push_back(v);
                start = i + 1;
            }
        }
        return out;
    };

    engine.registerFunction(
        "csvread",
        [resolveCsvPath, parseCsvLine](Span<const MValue> args,
                                       size_t nargout,
                                       Span<MValue> outs,
                                       CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty() || !args[0].isChar())
                throw MError("csvread requires a filename as the first argument");

            std::string filename = resolveCsvPath(args[0].toString());

            size_t r1 = 0, c1 = 0, r2 = 0, c2 = 0;
            bool haveRange = false;

            if (args.size() >= 2)
                r1 = static_cast<size_t>(args[1].toScalar());
            if (args.size() >= 3)
                c1 = static_cast<size_t>(args[2].toScalar());
            if (args.size() >= 4) {
                if (args[3].numel() != 4)
                    throw MError("csvread: range argument must be [R1 C1 R2 C2]");
                haveRange = true;
                r2 = static_cast<size_t>(args[3](2));
                c2 = static_cast<size_t>(args[3](3));
                if (r2 < r1 || c2 < c1)
                    throw MError("csvread: invalid range [R1 C1 R2 C2]");
            }

            auto resolved = ctx.engine->resolvePath(filename);
            std::string content;
            try {
                content = resolved.fs->readFile(resolved.path);
            } catch (const std::exception &e) {
                throw MError(std::string("csvread: ") + e.what());
            }

            std::vector<std::vector<double>> rows;
            size_t lineNo = 0;
            size_t pos = 0;
            while (pos <= content.size()) {
                size_t nl = content.find('\n', pos);
                size_t end = (nl == std::string::npos) ? content.size() : nl;
                // Stop cleanly on a trailing newline with nothing after it —
                // otherwise we'd produce one bogus empty row.
                if (end == pos && nl == std::string::npos)
                    break;
                std::string line = content.substr(pos, end - pos);
                pos = (nl == std::string::npos) ? content.size() + 1 : nl + 1;

                if (lineNo < r1) {
                    ++lineNo;
                    continue;
                }
                if (haveRange && lineNo > r2)
                    break;
                ++lineNo;

                std::vector<double> cells = parseCsvLine(line);
                std::vector<double> row;
                size_t endCol = haveRange ? std::min(c2 + 1, cells.size()) : cells.size();
                if (c1 < endCol)
                    row.assign(cells.begin() + c1, cells.begin() + endCol);
                rows.push_back(std::move(row));
            }

            size_t R, C;
            if (haveRange) {
                R = r2 - r1 + 1;
                C = c2 - c1 + 1;
                while (rows.size() < R)
                    rows.push_back({});
            } else {
                R = rows.size();
                C = 0;
                for (auto &r : rows)
                    if (r.size() > C)
                        C = r.size();
            }

            if (R == 0 || C == 0) {
                outs[0] = MValue::matrix(0, 0, MType::DOUBLE, alloc);
                return;
            }

            auto M = MValue::matrix(R, C, MType::DOUBLE, alloc);
            double *data = M.doubleDataMut();
            for (size_t r = 0; r < R; ++r) {
                const auto &row = rows[r];
                size_t n = std::min(C, row.size());
                for (size_t c = 0; c < n; ++c)
                    data[c * R + r] = row[c];
            }
            outs[0] = std::move(M);
        });

    engine.registerFunction(
        "csvwrite",
        [resolveCsvPath](Span<const MValue> args,
                         size_t nargout,
                         Span<MValue> outs,
                         CallContext &ctx) {
            if (args.size() < 2)
                throw MError("csvwrite requires filename and matrix arguments");
            if (!args[0].isChar())
                throw MError("csvwrite: first argument must be a filename");

            std::string filename = resolveCsvPath(args[0].toString());
            const MValue &M = args[1];

            size_t offR = 0, offC = 0;
            if (args.size() >= 3)
                offR = static_cast<size_t>(args[2].toScalar());
            if (args.size() >= 4)
                offC = static_cast<size_t>(args[3].toScalar());

            size_t R = M.dims().rows();
            size_t C = M.dims().cols();

            std::ostringstream os;

            for (size_t r = 0; r < offR; ++r)
                os << '\n';

            auto writeCell = [&](double v) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.15g", v);
                os << buf;
            };

            for (size_t r = 0; r < R; ++r) {
                for (size_t c = 0; c < offC; ++c)
                    os << ',';
                for (size_t c = 0; c < C; ++c) {
                    if (c > 0)
                        os << ',';
                    double v = 0.0;
                    if (M.type() == MType::DOUBLE) {
                        v = M.isScalar() ? M.toScalar() : M(r, c);
                    } else if (M.isLogical()) {
                        if (M.isScalar())
                            v = M.toBool() ? 1.0 : 0.0;
                        else
                            v = M.logicalData()[M.dims().sub2ind(r, c)] ? 1.0 : 0.0;
                    } else if (M.isComplex()) {
                        v = M.isScalar() ? M.toComplex().real() : M.complexElem(r, c).real();
                    }
                    writeCell(v);
                }
                os << '\n';
            }

            auto resolved = ctx.engine->resolvePath(filename);
            try {
                resolved.fs->writeFile(resolved.path, os.str());
            } catch (const std::exception &e) {
                throw MError(std::string("csvwrite: ") + e.what());
            }
        });

    // ── setenv / getenv ─────────────────────────────────────────
    //
    // MATLAB-compatible process-environment access. Session-scoped
    // (does not persist across restarts), same as MATLAB.
    //
    //   setenv(name)           → set name to empty string
    //   setenv(name, value)    → set name to value
    //   v = getenv(name)       → value, or '' if not set

    engine.registerFunction(
        "setenv",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            if (args.empty() || !args[0].isChar())
                throw MError("setenv: first argument must be a variable name");
            std::string name = args[0].toString();
            if (name.empty())
                throw MError("setenv: variable name cannot be empty");
            if (name.find('=') != std::string::npos)
                throw MError("setenv: variable name cannot contain '='");
            std::string value;
            if (args.size() >= 2) {
                if (!args[1].isChar())
                    throw MError("setenv: value must be a char array");
                value = args[1].toString();
            }
#ifdef _WIN32
            _putenv_s(name.c_str(), value.c_str());
#else
            ::setenv(name.c_str(), value.c_str(), 1);
#endif
        });

    engine.registerFunction(
        "getenv",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty() || !args[0].isChar())
                throw MError("getenv: argument must be a variable name");
            outs[0] = MValue::fromString(envGet(args[0].toString().c_str()), alloc);
        });

    // ── fopen / fclose ──────────────────────────────────────────
    //
    //   fid = fopen(filename)
    //   fid = fopen(filename, permission)   permission ∈ {'r','w','a'}
    //                                       ('t'/'b' suffixes accepted
    //                                        and ignored — we're binary)
    //   status = fclose(fid)                0 on success, -1 on failure
    //   status = fclose('all')              closes every user fid (>= 3)
    //
    // File ID is just a number; MATLAB contract is -1 on failure. The
    // file's content is accumulated in an Engine-owned buffer until
    // fclose flushes through the VirtualFS layer.

    // ── save / load (ascii) ─────────────────────────────────────
    //
    //   save(filename, var1 [, var2 …] [, '-ascii'])
    //   A = load(filename)
    //   load(filename)   — without LHS, assigns to a var named after
    //                      the file's stem (sans path + extension)
    //
    // Scope: MATLAB's -ascii text format only. Binary .mat isn't
    // plumbed; if no flag is given we still use ascii so simple
    // "save/load" workflows work. Callers asking for -mat get a clear
    // "not supported" error. Numeric (DOUBLE) matrices only; each row
    // of the matrix emits on its own line, columns separated by spaces,
    // 17-significant-digit precision (round-trip-safe for doubles).
    // Multiple vars in one save are concatenated with a blank line.

    engine.registerFunction(
        "save",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            (void)nargout; (void)outs;
            if (args.empty() || !args[0].isChar())
                throw MError("save: filename required");
            std::string filename = args[0].toString();

            bool asciiFlag = false;
            std::vector<std::string> varnames;
            for (size_t i = 1; i < args.size(); ++i) {
                if (!args[i].isChar())
                    continue;
                std::string s = args[i].toString();
                if (s == "-ascii") { asciiFlag = true; continue; }
                if (s == "-mat" || s == "-v7" || s == "-v7.3")
                    throw MError("save: binary .mat formats are not supported");
                if (!s.empty() && s.front() == '-')
                    throw MError("save: unsupported flag '" + s + "'");
                varnames.push_back(s);
            }
            (void)asciiFlag; // currently the only supported format

            if (varnames.empty())
                throw MError("save: at least one variable name is required");

            std::ostringstream out;
            for (size_t vi = 0; vi < varnames.size(); ++vi) {
                MValue *v = ctx.env->get(varnames[vi]);
                if (!v)
                    throw MError("save: variable '" + varnames[vi] + "' not found");
                if (v->type() != MType::DOUBLE)
                    throw MError("save: only numeric (double) variables supported in ascii mode");
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

            auto resolved = ctx.engine->resolvePath(filename);
            try {
                resolved.fs->writeFile(resolved.path, out.str());
            } catch (const std::exception &e) {
                throw MError(std::string("save: ") + e.what());
            }
        });

    engine.registerFunction(
        "load",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty() || !args[0].isChar())
                throw MError("load: filename required");
            std::string filename = args[0].toString();

            // Ignore -ascii flag; we only support ascii anyway.
            for (size_t i = 1; i < args.size(); ++i) {
                if (!args[i].isChar()) continue;
                std::string s = args[i].toString();
                if (s == "-mat" || s == "-v7" || s == "-v7.3")
                    throw MError("load: binary .mat formats are not supported");
                if (s == "-ascii") continue;
                if (!s.empty() && s.front() == '-')
                    throw MError("load: unsupported flag '" + s + "'");
            }

            auto resolved = ctx.engine->resolvePath(filename);
            std::string content;
            try {
                content = resolved.fs->readFile(resolved.path);
            } catch (const std::exception &e) {
                throw MError(std::string("load: ") + e.what());
            }

            // Parse each non-empty, non-comment line as whitespace-separated
            // doubles. MATLAB ignores '%' and '#' line comments.
            std::vector<std::vector<double>> rows;
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

                std::vector<double> row;
                size_t q = ws;
                while (q < line.size()) {
                    while (q < line.size() && std::isspace(static_cast<unsigned char>(line[q]))) ++q;
                    if (q >= line.size()) break;
                    const char *start = line.c_str() + q;
                    char *endp = nullptr;
                    double v = std::strtod(start, &endp);
                    if (endp == start)
                        throw MError("load: parse error near '" + line.substr(q) + "'");
                    row.push_back(v);
                    q = static_cast<size_t>(endp - line.c_str());
                }
                rows.push_back(std::move(row));
            }

            if (rows.empty())
                throw MError("load: no numeric data found");
            size_t cols = rows[0].size();
            for (auto &r : rows) {
                if (r.size() != cols)
                    throw MError("load: inconsistent column count across rows");
            }
            size_t nrows = rows.size();

            MValue M;
            if (nrows == 1 && cols == 1) {
                M = MValue::scalar(rows[0][0], alloc);
            } else {
                M = MValue::matrix(nrows, cols, MType::DOUBLE, alloc);
                double *data = M.doubleDataMut();
                for (size_t r = 0; r < nrows; ++r)
                    for (size_t c = 0; c < cols; ++c)
                        data[c * nrows + r] = rows[r][c];
            }

            if (nargout > 0) {
                outs[0] = std::move(M);
                return;
            }

            // No LHS — MATLAB assigns to a variable named after the
            // file's stem (basename without directory and extension).
            std::string stem = filename;
            size_t sep = stem.find_last_of("/\\:");
            if (sep != std::string::npos) stem = stem.substr(sep + 1);
            size_t dot = stem.find_last_of('.');
            if (dot != std::string::npos && dot > 0) stem = stem.substr(0, dot);
            if (stem.empty() || !(std::isalpha(static_cast<unsigned char>(stem[0])) || stem[0] == '_'))
                throw MError("load: cannot derive a valid variable name from filename");
            ctx.env->set(stem, std::move(M));
        });

    // fopen / fclose / fgetl / fgets / feof / ferror / ftell / fseek /
    // frewind now live in libs/builtin/src/MStdFileIO.cpp.

    // fread / fwrite now live in libs/builtin/src/MStdFileIO.cpp. The
    // shared parsing helpers (SizeSpec, parseReadSize, shapeFreadOutput,
    // parsePrecision, parseEndian, byteSwap) live in MStdIOHelpers.hpp.

    // fscanf / sscanf / textscan now live in libs/builtin/src/MStdScan.cpp.
}

} // namespace numkit::m