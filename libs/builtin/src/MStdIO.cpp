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
    // csvread / csvwrite / setenv / getenv live in
    // libs/builtin/src/MStdEnvCsv.cpp.

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