#include "MLabBranding.hpp"
#include "MLabStdLibrary.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <vector>

namespace mlab {

void StdLibrary::registerIOFunctions(Engine &engine)
{
    engine.registerFunction("disp",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                for (auto &a : args) {
                                    std::ostringstream os;
                                    if (a.isChar()) {
                                        os << a.toString();
                                    } else if (a.isEmpty()) {
                                        os << "[]";
                                    } else if (a.type() == MType::DOUBLE) {
                                        if (a.isScalar()) {
                                            os << a.toScalar();
                                        } else {
                                            auto d = a.dims();
                                            if (d.rows() == 1) {
                                                os << "[";
                                                for (size_t c = 0; c < d.cols(); ++c) {
                                                    if (c > 0)
                                                        os << " ";
                                                    double v = a(0, c);
                                                    if (v == std::floor(v) && std::isfinite(v))
                                                        os << static_cast<long long>(v);
                                                    else
                                                        os << v;
                                                }
                                                os << "]";
                                            } else if (d.cols() == 1) {
                                                for (size_t r = 0; r < d.rows(); ++r) {
                                                    if (r > 0)
                                                        os << "\n";
                                                    double v = a(r, 0);
                                                    if (v == std::floor(v) && std::isfinite(v))
                                                        os << "   " << static_cast<long long>(v);
                                                    else
                                                        os << "   " << v;
                                                }
                                            } else {
                                                for (size_t p = 0; p < d.pages(); ++p) {
                                                    if (d.is3D())
                                                        os << "(:,:," << p + 1 << ") =\n";
                                                    for (size_t r = 0; r < d.rows(); ++r) {
                                                        if (r > 0)
                                                            os << "\n";
                                                        os << "   ";
                                                        for (size_t c = 0; c < d.cols(); ++c) {
                                                            double v = a(r, c, p);
                                                            if (v == std::floor(v)
                                                                && std::isfinite(v))
                                                                os << " "
                                                                   << static_cast<long long>(v);
                                                            else
                                                                os << " " << v;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    } else if (a.isLogical()) {
                                        if (a.isScalar()) {
                                            os << (a.toBool() ? "1" : "0");
                                        } else {
                                            auto d = a.dims();
                                            const uint8_t *ld = a.logicalData();
                                            for (size_t r = 0; r < d.rows(); ++r) {
                                                if (r > 0)
                                                    os << "\n";
                                                os << "   ";
                                                for (size_t c = 0; c < d.cols(); ++c)
                                                    os << " " << (ld[d.sub2ind(r, c)] ? "1" : "0");
                                            }
                                        }
                                    } else if (a.isStruct()) {
                                        for (auto &[k, v] : a.structFields())
                                            os << "    " << k << ": " << v.debugString() << "\n";
                                    } else if (a.isCell()) {
                                        auto d = a.dims();
                                        os << "{" << d.rows() << "x" << d.cols() << " cell}";
                                    } else if (a.isComplex()) {
                                        if (a.isScalar()) {
                                            auto c = a.toComplex();
                                            if (c.real() != 0.0 || c.imag() == 0.0)
                                                os << c.real();
                                            if (c.imag() != 0.0) {
                                                if (c.real() != 0.0 && c.imag() > 0)
                                                    os << "+";
                                                os << c.imag() << "i";
                                            }
                                        } else {
                                            os << a.debugString();
                                        }
                                    } else {
                                        os << a.debugString();
                                    }
                                    os << "\n";
                                    ctx.engine->outputText(os.str());
                                }
                                return;
                            });

    // ── shared printf-style formatter ──────────────────────────
    auto mlab_sprintf =
        [](const std::string &fmt, Span<const MValue> args, size_t argStart) -> std::string {
        std::ostringstream out;
        size_t ai = argStart;

        for (size_t i = 0; i < fmt.size(); ++i) {
            if (fmt[i] == '\\' && i + 1 < fmt.size()) {
                char next = fmt[i + 1];
                if (next == 'n') {
                    out << '\n';
                    i++;
                    continue;
                }
                if (next == 't') {
                    out << '\t';
                    i++;
                    continue;
                }
                if (next == '\\') {
                    out << '\\';
                    i++;
                    continue;
                }
                if (next == '\'') {
                    out << '\'';
                    i++;
                    continue;
                }
                out << fmt[i];
                continue;
            }

            if (fmt[i] == '%') {
                if (i + 1 < fmt.size() && fmt[i + 1] == '%') {
                    out << '%';
                    i++;
                    continue;
                }

                size_t start = i;
                i++;

                while (i < fmt.size()
                       && (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == '0' || fmt[i] == ' '
                           || fmt[i] == '#'))
                    i++;
                if (i < fmt.size() && fmt[i] == '*') {
                    i++;
                } else {
                    while (i < fmt.size() && fmt[i] >= '0' && fmt[i] <= '9')
                        i++;
                }
                if (i < fmt.size() && fmt[i] == '.') {
                    i++;
                    if (i < fmt.size() && fmt[i] == '*') {
                        i++;
                    } else {
                        while (i < fmt.size() && fmt[i] >= '0' && fmt[i] <= '9')
                            i++;
                    }
                }
                while (i < fmt.size() && (fmt[i] == 'l' || fmt[i] == 'h'))
                    i++;

                if (i >= fmt.size())
                    break;

                char type = fmt[i];
                std::string spec(fmt, start, i - start + 1);

                if (type == 's') {
                    if (ai < args.size() && args[ai].isChar())
                        out << args[ai].toString();
                    ai++;
                } else if (type == 'c') {
                    if (ai < args.size()) {
                        if (args[ai].isChar()) {
                            std::string s = args[ai].toString();
                            out << (s.empty() ? ' ' : s[0]);
                        } else {
                            out << static_cast<char>(static_cast<int>(args[ai].toScalar()));
                        }
                    }
                    ai++;
                } else if (type == 'd' || type == 'i') {
                    if (ai < args.size()) {
                        char buf[64];
                        std::string ispec = spec.substr(0, spec.size() - 1) + "lld";
                        std::snprintf(buf,
                                      sizeof(buf),
                                      ispec.c_str(),
                                      static_cast<long long>(args[ai].toScalar()));
                        out << buf;
                    }
                    ai++;
                } else if (type == 'u') {
                    if (ai < args.size()) {
                        char buf[64];
                        std::string uspec = spec.substr(0, spec.size() - 1) + "llu";
                        std::snprintf(buf,
                                      sizeof(buf),
                                      uspec.c_str(),
                                      static_cast<unsigned long long>(args[ai].toScalar()));
                        out << buf;
                    }
                    ai++;
                } else if (type == 'x' || type == 'X') {
                    if (ai < args.size()) {
                        char buf[64];
                        std::string xspec = spec.substr(0, spec.size() - 1) + "ll" + type;
                        std::snprintf(buf,
                                      sizeof(buf),
                                      xspec.c_str(),
                                      static_cast<unsigned long long>(args[ai].toScalar()));
                        out << buf;
                    }
                    ai++;
                } else if (type == 'o') {
                    if (ai < args.size()) {
                        char buf[64];
                        std::string ospec = spec.substr(0, spec.size() - 1) + "llo";
                        std::snprintf(buf,
                                      sizeof(buf),
                                      ospec.c_str(),
                                      static_cast<unsigned long long>(args[ai].toScalar()));
                        out << buf;
                    }
                    ai++;
                } else if (type == 'f' || type == 'e' || type == 'E' || type == 'g' || type == 'G') {
                    if (ai < args.size()) {
                        char buf[128];
                        std::snprintf(buf, sizeof(buf), spec.c_str(), args[ai].toScalar());
                        out << buf;
                    }
                    ai++;
                } else {
                    out << spec;
                }
                continue;
            }

            out << fmt[i];
        }
        return out.str();
    };

    engine.registerFunction(
        "fprintf",
        [mlab_sprintf](Span<const MValue> args,
                       size_t nargout,
                       Span<MValue> outs,
                       CallContext &ctx) {
            if (args.empty())
                return;

            // First-arg-is-fid disambiguation: MATLAB allows both
            // fprintf(format, …) and fprintf(fid, format, …); we detect
            // the latter by a leading numeric scalar with a char next.
            int fid = 1; // default → stdout
            size_t fmtIdx = 0;
            if (args.size() >= 2 && args[0].isScalar() && args[1].isChar()) {
                fid = static_cast<int>(args[0].toScalar());
                fmtIdx = 1;
            }
            if (!args[fmtIdx].isChar())
                return;

            std::string result = mlab_sprintf(args[fmtIdx].toString(), args, fmtIdx + 1);

            if (fid == 1 || fid == 2) {
                ctx.engine->outputText(result);
            } else if (fid >= 3) {
                auto *f = ctx.engine->findFile(fid);
                if (!f || !f->forWrite)
                    throw MLabError("fprintf: invalid file identifier");
                f->buffer.append(result);
            } else {
                throw MLabError("fprintf: invalid file identifier");
            }
        });

    engine.registerFunction("sprintf",
                            [mlab_sprintf](Span<const MValue> args,
                                           size_t nargout,
                                           Span<MValue> outs,
                                           CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.empty() || !args[0].isChar()) {
                                    outs[0] = MValue::fromString("", alloc);
                                    return;
                                }
                                std::string result = mlab_sprintf(args[0].toString(), args, 1);
                                {
                                    outs[0] = MValue::fromString(result, alloc);
                                    return;
                                }
                            });

    engine.registerFunction(
        "error",
        [mlab_sprintf](Span<const MValue> args, size_t nargout, Span<MValue> outs,
                        CallContext &ctx) {
            if (args.empty())
                throw MLabError("Error");

            // error(struct) — rethrow an MException-like struct
            if (args[0].isStruct()) {
                std::string msg = args[0].hasField("message") ? args[0].field("message").toString()
                                                              : "Error";
                std::string id = args[0].hasField("identifier")
                                     ? args[0].field("identifier").toString()
                                     : "";
                throw MLabError(msg, 0, 0, "", "", id);
            }

            std::string first = args[0].toString();

            // Detect error(id, msg, ...) vs error(msg, ...)
            // MATLAB: if first arg contains ':', it's an identifier
            if (args.size() >= 2 && first.find(':') != std::string::npos
                && (args[1].isChar() || args[1].isString())) {
                std::string id = first;
                std::string msg;
                if (args.size() > 2)
                    msg = mlab_sprintf(args[1].toString(), args, 2);
                else
                    msg = args[1].toString();
                throw MLabError(msg, 0, 0, "", "", id);
            }

            // error(msg) or error(msg, arg1, ...) — sprintf formatting
            std::string msg;
            if (args.size() > 1)
                msg = mlab_sprintf(first, args, 1);
            else
                msg = first;
            throw MLabError(msg);
        });

    engine.registerFunction(
        "warning",
        [mlab_sprintf](Span<const MValue> args, size_t nargout, Span<MValue> outs,
                        CallContext &ctx) {
            if (args.empty())
                return;
            std::string msg;
            // warning(id, msg, ...) or warning(msg, ...)
            std::string first = args[0].toString();
            if (args.size() >= 2 && first.find(':') != std::string::npos
                && (args[1].isChar() || args[1].isString())) {
                // warning(id, msg, ...) — skip identifier, format message
                if (args.size() > 2)
                    msg = mlab_sprintf(args[1].toString(), args, 2);
                else
                    msg = args[1].toString();
            } else if (args.size() > 1) {
                msg = mlab_sprintf(first, args, 1);
            } else {
                msg = first;
            }
            ctx.engine->outputText("Warning: " + msg + "\n");
        });

    // MException(id, msg, ...) — create exception struct
    engine.registerFunction(
        "MException",
        [mlab_sprintf](Span<const MValue> args, size_t nargout, Span<MValue> outs,
                        CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() < 2)
                throw std::runtime_error("MException requires identifier and message");
            std::string id = args[0].toString();
            std::string msg;
            if (args.size() > 2)
                msg = mlab_sprintf(args[1].toString(), args, 2);
            else
                msg = args[1].toString();
            auto me = MValue::structure();
            me.field("identifier") = MValue::fromString(id, alloc);
            me.field("message") = MValue::fromString(msg, alloc);
            outs[0] = std::move(me);
        });

    // rethrow(ME) — rethrow a caught exception struct
    engine.registerFunction(
        "rethrow",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            if (args.empty() || !args[0].isStruct())
                throw std::runtime_error("rethrow requires an MException struct");
            const auto &me = args[0];
            std::string msg = me.hasField("message") ? me.field("message").toString() : "Error";
            std::string id =
                me.hasField("identifier") ? me.field("identifier").toString() : "MLAB:error";
            throw MLabError(msg, 0, 0, "", "", id);
        });

    // throw(ME) — alias for rethrow
    engine.registerFunction(
        "throw",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            if (args.empty() || !args[0].isStruct())
                throw std::runtime_error("throw requires an MException struct");
            const auto &me = args[0];
            std::string msg = me.hasField("message") ? me.field("message").toString() : "Error";
            std::string id =
                me.hasField("identifier") ? me.field("identifier").toString() : "MLAB:error";
            throw MLabError(msg, 0, 0, "", "", id);
        });

    // assert(condition) / assert(condition, msg) / assert(condition, id, msg, ...)
    engine.registerFunction(
        "assert",
        [mlab_sprintf](Span<const MValue> args, size_t nargout, Span<MValue> outs,
                        CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("assert requires at least one argument");
            if (args[0].toBool())
                return; // assertion passed
            if (args.size() == 1)
                throw MLabError("Assertion failed.", 0, 0, "", "", "MLAB:assert");
            // assert(cond, MException struct)
            if (args[1].isStruct()) {
                std::string msg = args[1].hasField("message")
                                      ? args[1].field("message").toString()
                                      : "Assertion failed.";
                std::string id = args[1].hasField("identifier")
                                     ? args[1].field("identifier").toString()
                                     : "MLAB:assert";
                throw MLabError(msg, 0, 0, "", "", id);
            }
            // assert(cond, msg) or assert(cond, id, msg, ...)
            std::string first = args[1].toString();
            if (args.size() >= 3 && first.find(':') != std::string::npos) {
                std::string id = first;
                std::string msg;
                if (args.size() > 3)
                    msg = mlab_sprintf(args[2].toString(), args, 3);
                else
                    msg = args[2].toString();
                throw MLabError(msg, 0, 0, "", "", id);
            }
            std::string msg;
            if (args.size() > 2)
                msg = mlab_sprintf(first, args, 2);
            else
                msg = first;
            throw MLabError(msg, 0, 0, "", "", "MLAB:assert");
        });

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
                throw MLabError("csvread requires a filename as the first argument");

            std::string filename = resolveCsvPath(args[0].toString());

            size_t r1 = 0, c1 = 0, r2 = 0, c2 = 0;
            bool haveRange = false;

            if (args.size() >= 2)
                r1 = static_cast<size_t>(args[1].toScalar());
            if (args.size() >= 3)
                c1 = static_cast<size_t>(args[2].toScalar());
            if (args.size() >= 4) {
                if (args[3].numel() != 4)
                    throw MLabError("csvread: range argument must be [R1 C1 R2 C2]");
                haveRange = true;
                r2 = static_cast<size_t>(args[3](2));
                c2 = static_cast<size_t>(args[3](3));
                if (r2 < r1 || c2 < c1)
                    throw MLabError("csvread: invalid range [R1 C1 R2 C2]");
            }

            auto resolved = ctx.engine->resolvePath(filename);
            std::string content;
            try {
                content = resolved.fs->readFile(resolved.path);
            } catch (const std::exception &e) {
                throw MLabError(std::string("csvread: ") + e.what());
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
                throw MLabError("csvwrite requires filename and matrix arguments");
            if (!args[0].isChar())
                throw MLabError("csvwrite: first argument must be a filename");

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
                throw MLabError(std::string("csvwrite: ") + e.what());
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
                throw MLabError("setenv: first argument must be a variable name");
            std::string name = args[0].toString();
            if (name.empty())
                throw MLabError("setenv: variable name cannot be empty");
            if (name.find('=') != std::string::npos)
                throw MLabError("setenv: variable name cannot contain '='");
            std::string value;
            if (args.size() >= 2) {
                if (!args[1].isChar())
                    throw MLabError("setenv: value must be a char array");
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
                throw MLabError("getenv: argument must be a variable name");
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

    engine.registerFunction(
        "fopen",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty() || !args[0].isChar())
                throw MLabError("fopen: filename must be a char array");
            std::string path = args[0].toString();
            std::string mode = (args.size() >= 2 && args[1].isChar()) ? args[1].toString() : "r";
            int fid = ctx.engine->openFile(path, mode);
            outs[0] = MValue::scalar(static_cast<double>(fid), alloc);
        });

    engine.registerFunction(
        "fclose",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty())
                throw MLabError("fclose: requires a file identifier or 'all'");

            if (args[0].isChar() && args[0].toString() == "all") {
                ctx.engine->closeAllFiles();
                outs[0] = MValue::scalar(0.0, alloc);
                return;
            }

            if (!args[0].isScalar())
                throw MLabError("fclose: argument must be a numeric fid or 'all'");
            int fid = static_cast<int>(args[0].toScalar());
            bool ok = ctx.engine->closeFile(fid);
            outs[0] = MValue::scalar(ok ? 0.0 : -1.0, alloc);
        });

    // ── fgetl / fgets / feof ────────────────────────────────────
    //
    //   tline = fgetl(fid)          read next line, strip trailing \n (and \r).
    //                               Returns -1 (double) at EOF, per MATLAB.
    //   tline = fgets(fid)          same, but keeps the terminator.
    //   tline = fgets(fid, nchar)   up to nchar chars or until newline.
    //   tf    = feof(fid)           logical 1 if cursor is past end, else 0.
    //
    // fgetl/fgets require fids opened for reading ('r'). Writing fids
    // throw; the 'a'/'w' user presumably did not intend to read.

    auto requireReadFid = [](mlab::Engine *eng, const Span<const MValue> &args,
                             const char *fn) -> mlab::Engine::OpenFile * {
        if (args.empty() || !args[0].isScalar())
            throw MLabError(std::string(fn) + ": file identifier required");
        int fid = static_cast<int>(args[0].toScalar());
        auto *f = eng->findFile(fid);
        if (!f || !f->forRead)
            throw MLabError(std::string(fn) + ": invalid file identifier");
        return f;
    };

    engine.registerFunction(
        "fgetl",
        [requireReadFid](Span<const MValue> args, size_t nargout, Span<MValue> outs,
                         CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto *f = requireReadFid(ctx.engine, args, "fgetl");

            if (f->cursor >= f->buffer.size()) {
                outs[0] = MValue::scalar(-1.0, alloc);
                return;
            }
            size_t start = f->cursor;
            size_t nl = f->buffer.find('\n', start);
            size_t end = (nl == std::string::npos) ? f->buffer.size() : nl;
            // Strip trailing \r so CRLF files don't leak the carriage
            // return into the returned line — MATLAB does the same.
            size_t trimEnd = end;
            if (trimEnd > start && f->buffer[trimEnd - 1] == '\r')
                --trimEnd;
            std::string line = f->buffer.substr(start, trimEnd - start);
            f->cursor = (nl == std::string::npos) ? f->buffer.size() : nl + 1;
            outs[0] = MValue::fromString(line, alloc);
        });

    engine.registerFunction(
        "fgets",
        [requireReadFid](Span<const MValue> args, size_t nargout, Span<MValue> outs,
                         CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto *f = requireReadFid(ctx.engine, args, "fgets");

            if (f->cursor >= f->buffer.size()) {
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
        });

    engine.registerFunction(
        "feof",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty() || !args[0].isScalar())
                throw MLabError("feof: file identifier required");
            int fid = static_cast<int>(args[0].toScalar());
            auto *f = ctx.engine->findFile(fid);
            if (!f)
                throw MLabError("feof: invalid file identifier");
            outs[0] = MValue::logicalScalar(f->cursor >= f->buffer.size(), alloc);
        });

    // ── ftell / fseek / frewind ─────────────────────────────────
    //
    //   pos    = ftell(fid)                    byte position from start
    //                                          (0-based). -1 on failure.
    //   status = fseek(fid, offset, origin)    origin ∈ {'bof','cof','eof'}
    //                                          or the ints {-1, 0, 1}.
    //                                          0 on success, -1 on failure.
    //   frewind(fid)                           shortcut for fseek(fid,0,'bof')
    //
    // We support seek on read-mode fids (moves the read cursor). Write-
    // mode fids are append-only in our buffer model, so fseek returns
    // -1 there — a safer stub than silently ignoring the seek.

    engine.registerFunction(
        "ftell",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty() || !args[0].isScalar())
                throw MLabError("ftell: file identifier required");
            int fid = static_cast<int>(args[0].toScalar());
            auto *f = ctx.engine->findFile(fid);
            if (!f) {
                outs[0] = MValue::scalar(-1.0, alloc);
                return;
            }
            // Write-mode: report end-of-buffer (where next append lands).
            size_t pos = f->forWrite ? f->buffer.size() : f->cursor;
            outs[0] = MValue::scalar(static_cast<double>(pos), alloc);
        });

    engine.registerFunction(
        "fseek",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto fail = [&]() { outs[0] = MValue::scalar(-1.0, alloc); };

            if (args.size() < 2 || !args[0].isScalar() || !args[1].isScalar())
                return fail();

            int fid = static_cast<int>(args[0].toScalar());
            auto *f = ctx.engine->findFile(fid);
            if (!f || !f->forRead)
                return fail();

            long long offset = static_cast<long long>(args[1].toScalar());

            // Origin: defaults to 'bof' if omitted (MATLAB allows). Accepts
            // either a string ('bof'/'cof'/'eof') or the int codes
            // -1/0/1 used by some MATLAB-equivalent APIs.
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
        });

    engine.registerFunction(
        "frewind",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            if (args.empty() || !args[0].isScalar())
                throw MLabError("frewind: file identifier required");
            int fid = static_cast<int>(args[0].toScalar());
            auto *f = ctx.engine->findFile(fid);
            if (!f || !f->forRead)
                throw MLabError("frewind: invalid file identifier");
            f->cursor = 0;
        });

    // ── fread / fwrite ──────────────────────────────────────────
    //
    // Binary I/O over open fids. MATLAB-compatible signatures:
    //
    //   A             = fread(fid)
    //   A             = fread(fid, size)
    //   A             = fread(fid, size, precision)
    //   [A, count]    = fread(fid, ...)
    //
    //   count = fwrite(fid, A)
    //   count = fwrite(fid, A, precision)
    //
    // size ∈ {scalar N, Inf} — return at most that many elements. Inf
    // or omitted reads everything remaining. Matrix-shape `[m n]` not
    // supported yet; callers can reshape after.
    //
    // Supported precisions (with aliases): uint8/uchar/char, int8/schar,
    // uint16, int16, uint32, int32, uint64, int64, single/float32,
    // double/float64. Typed-output syntax "src=>dst" is accepted but
    // the destination part is ignored — our arrays are always double.
    //
    // Endianness: little-endian on both ends — matches WASM + every
    // common host architecture we target. Explicit machine-format
    // override is NOT supported.

    // clang-format off
    // Returns {kind, bytes} for a MATLAB precision spec, or nullopt.
    auto parsePrecision = [](const std::string &raw) -> std::optional<std::pair<int, size_t>> {
        // kind: 0 = uint, 1 = int, 2 = float
        std::string p = raw;
        auto arrow = p.find("=>");
        if (arrow != std::string::npos) p = p.substr(0, arrow);
        // trim whitespace
        while (!p.empty() && std::isspace(static_cast<unsigned char>(p.front()))) p.erase(0, 1);
        while (!p.empty() && std::isspace(static_cast<unsigned char>(p.back()))) p.pop_back();

        if (p == "uint8" || p == "uchar" || p == "char") return std::make_pair(0, size_t{1});
        if (p == "int8"  || p == "schar")                return std::make_pair(1, size_t{1});
        if (p == "uint16")                               return std::make_pair(0, size_t{2});
        if (p == "int16")                                return std::make_pair(1, size_t{2});
        if (p == "uint32")                               return std::make_pair(0, size_t{4});
        if (p == "int32")                                return std::make_pair(1, size_t{4});
        if (p == "uint64")                               return std::make_pair(0, size_t{8});
        if (p == "int64")                                return std::make_pair(1, size_t{8});
        if (p == "single" || p == "float32" || p == "real*4") return std::make_pair(2, size_t{4});
        if (p == "double" || p == "float64" || p == "real*8") return std::make_pair(2, size_t{8});
        return std::nullopt;
    };
    // clang-format on

    engine.registerFunction(
        "fread",
        [parsePrecision](Span<const MValue> args, size_t nargout, Span<MValue> outs,
                         CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty() || !args[0].isScalar())
                throw MLabError("fread: file identifier required");
            int fid = static_cast<int>(args[0].toScalar());
            auto *f = ctx.engine->findFile(fid);
            if (!f || !f->forRead)
                throw MLabError("fread: invalid file identifier");

            // size argument
            size_t requested = 0;
            bool readAll = true;
            if (args.size() >= 2) {
                double s = args[1].toScalar();
                if (std::isinf(s)) {
                    readAll = true;
                } else if (s < 0 || !std::isfinite(s)) {
                    throw MLabError("fread: size must be Inf or a non-negative integer");
                } else {
                    readAll = false;
                    requested = static_cast<size_t>(s);
                }
            }

            // precision argument
            std::string precStr = "uint8";
            if (args.size() >= 3) {
                if (!args[2].isChar())
                    throw MLabError("fread: precision must be a char array");
                precStr = args[2].toString();
            }
            auto precOpt = parsePrecision(precStr);
            if (!precOpt)
                throw MLabError("fread: unsupported precision '" + precStr + "'");
            int kind = precOpt->first;
            size_t bsize = precOpt->second;

            size_t available = f->buffer.size() - f->cursor;
            size_t maxElems = available / bsize;
            size_t n = readAll ? maxElems : std::min(requested, maxElems);

            // Always return a column vector of doubles — MATLAB's
            // default output type regardless of precision.
            MValue M = MValue::matrix(std::max<size_t>(n, 0), n == 0 ? 0 : 1, MType::DOUBLE, alloc);
            if (n > 0) {
                double *data = M.doubleDataMut();
                for (size_t i = 0; i < n; ++i) {
                    const char *src = f->buffer.data() + f->cursor + i * bsize;
                    double v = 0.0;
                    if (kind == 0) { // unsigned
                        switch (bsize) {
                        case 1: { uint8_t  x; std::memcpy(&x, src, 1); v = x; break; }
                        case 2: { uint16_t x; std::memcpy(&x, src, 2); v = x; break; }
                        case 4: { uint32_t x; std::memcpy(&x, src, 4); v = x; break; }
                        case 8: { uint64_t x; std::memcpy(&x, src, 8); v = static_cast<double>(x); break; }
                        }
                    } else if (kind == 1) { // signed
                        switch (bsize) {
                        case 1: { int8_t  x; std::memcpy(&x, src, 1); v = x; break; }
                        case 2: { int16_t x; std::memcpy(&x, src, 2); v = x; break; }
                        case 4: { int32_t x; std::memcpy(&x, src, 4); v = x; break; }
                        case 8: { int64_t x; std::memcpy(&x, src, 8); v = static_cast<double>(x); break; }
                        }
                    } else { // float
                        if (bsize == 4) { float x; std::memcpy(&x, src, 4); v = x; }
                        else            {                     std::memcpy(&v, src, 8); }
                    }
                    data[i] = v;
                }
            }
            f->cursor += n * bsize;
            outs[0] = std::move(M);
            if (nargout > 1)
                outs[1] = MValue::scalar(static_cast<double>(n), alloc);
        });

    engine.registerFunction(
        "fwrite",
        [parsePrecision](Span<const MValue> args, size_t nargout, Span<MValue> outs,
                         CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() < 2 || !args[0].isScalar())
                throw MLabError("fwrite: requires (fid, array [, precision])");
            int fid = static_cast<int>(args[0].toScalar());
            auto *f = ctx.engine->findFile(fid);
            if (!f || !f->forWrite)
                throw MLabError("fwrite: invalid file identifier");

            std::string precStr = "uint8";
            if (args.size() >= 3) {
                if (!args[2].isChar())
                    throw MLabError("fwrite: precision must be a char array");
                precStr = args[2].toString();
            }
            auto precOpt = parsePrecision(precStr);
            if (!precOpt)
                throw MLabError("fwrite: unsupported precision '" + precStr + "'");
            int kind = precOpt->first;
            size_t bsize = precOpt->second;

            const MValue &A = args[1];
            size_t numel = A.numel();

            // Small helper: read i-th element as double from a double or
            // logical source. Other numeric types could be added here.
            auto elemAsDouble = [&A](size_t i) -> double {
                if (A.type() == MType::DOUBLE)  return A.doubleData()[i];
                if (A.isLogical())              return A.logicalData()[i] ? 1.0 : 0.0;
                throw MLabError("fwrite: unsupported array element type");
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
                    else            {                                   std::memcpy(dst, &v, 8); }
                }
                dst += bsize;
            }
            f->buffer.append(bytes);
            outs[0] = MValue::scalar(static_cast<double>(numel), alloc);
        });
}

} // namespace mlab