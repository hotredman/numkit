#include "MBranding.hpp"
#include "MStdLibrary.hpp"

#include <array>
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
                                        // Row vector (or 1×0): print as a single line.
                                        // Matrix: print each row on its own line —
                                        // MATLAB disp() parity.
                                        auto d = a.dims();
                                        if (d.rows() <= 1) {
                                            os << a.toString();
                                        } else {
                                            for (size_t r = 0; r < d.rows(); ++r) {
                                                if (r > 0) os << "\n";
                                                os << a.charRow(r);
                                            }
                                        }
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

    // MATLAB fprintf / sprintf apply the format string CYCLICALLY over
    // array inputs, column-major: fprintf('%d %d\n', [1 2 3 4]) prints
    //   1 2
    //   3 4
    // We flatten numeric array args into a stream of scalar MValues and
    // invoke mlab_sprintf once per chunk of (nSpecs) values. Char args
    // stay as single placeholders (MATLAB's %s takes the whole string).
    auto countFormatSpecs = [](const std::string &fmt) -> size_t {
        size_t n = 0;
        for (size_t i = 0; i < fmt.size(); ++i) {
            if (fmt[i] != '%') continue;
            if (i + 1 < fmt.size() && fmt[i + 1] == '%') { ++i; continue; } // literal %%
            ++i;
            while (i < fmt.size() &&
                   (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == '0' ||
                    fmt[i] == ' ' || fmt[i] == '#' || fmt[i] == '.' ||
                    std::isdigit(static_cast<unsigned char>(fmt[i]))))
                ++i;
            while (i < fmt.size() && (fmt[i] == 'l' || fmt[i] == 'h'))
                ++i;
            if (i < fmt.size()) ++n;
        }
        return n;
    };

    auto sprintfCyclic = [mlab_sprintf, countFormatSpecs](
                             const std::string &fmt, Span<const MValue> args, size_t argStart,
                             Allocator *alloc) -> std::string {
        std::vector<MValue> stream;
        stream.reserve(args.size() - argStart);
        for (size_t i = argStart; i < args.size(); ++i) {
            const MValue &a = args[i];
            if (a.isChar() || a.isScalar()) {
                stream.push_back(a);
                continue;
            }
            size_t n = a.numel();
            for (size_t j = 0; j < n; ++j) {
                double v;
                if (a.type() == MType::DOUBLE) v = a.doubleData()[j];
                else if (a.isLogical())        v = a.logicalData()[j] ? 1.0 : 0.0;
                else                           v = a(j);
                stream.push_back(MValue::scalar(v, alloc));
            }
        }

        size_t nSpecs = countFormatSpecs(fmt);
        if (nSpecs == 0 || stream.size() <= nSpecs) {
            return mlab_sprintf(fmt, Span<const MValue>{stream.data(), stream.size()}, 0);
        }
        std::string out;
        size_t pos = 0;
        while (pos < stream.size()) {
            size_t end = std::min(pos + nSpecs, stream.size());
            out += mlab_sprintf(
                fmt, Span<const MValue>{stream.data() + pos, end - pos}, 0);
            pos = end;
        }
        return out;
    };

    engine.registerFunction(
        "fprintf",
        [sprintfCyclic](Span<const MValue> args,
                        size_t nargout,
                        Span<MValue> outs,
                        CallContext &ctx) {
            if (args.empty())
                return;
            auto *alloc = &ctx.engine->allocator();

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

            std::string result = sprintfCyclic(args[fmtIdx].toString(), args, fmtIdx + 1, alloc);

            if (fid == 1 || fid == 2) {
                ctx.engine->outputText(result);
            } else if (fid >= 3) {
                auto *f = ctx.engine->findFile(fid);
                if (!f || !f->forWrite)
                    throw MLabError("fprintf: invalid file identifier");
                // Write at cursor, extending the buffer if needed. For
                // 'a'/'a+' (appendOnly) we snap to the end first —
                // MATLAB's contract regardless of prior seek.
                size_t writePos = f->appendOnly ? f->buffer.size() : f->cursor;
                if (writePos + result.size() > f->buffer.size())
                    f->buffer.resize(writePos + result.size());
                std::memcpy(f->buffer.data() + writePos, result.data(), result.size());
                f->cursor = writePos + result.size();
            } else {
                throw MLabError("fprintf: invalid file identifier");
            }
        });

    engine.registerFunction("sprintf",
                            [sprintfCyclic](Span<const MValue> args,
                                            size_t nargout,
                                            Span<MValue> outs,
                                            CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.empty() || !args[0].isChar()) {
                                    outs[0] = MValue::fromString("", alloc);
                                    return;
                                }
                                std::string result =
                                    sprintfCyclic(args[0].toString(), args, 1, alloc);
                                outs[0] = MValue::fromString(result, alloc);
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
                throw MLabError("save: filename required");
            std::string filename = args[0].toString();

            bool asciiFlag = false;
            std::vector<std::string> varnames;
            for (size_t i = 1; i < args.size(); ++i) {
                if (!args[i].isChar())
                    continue;
                std::string s = args[i].toString();
                if (s == "-ascii") { asciiFlag = true; continue; }
                if (s == "-mat" || s == "-v7" || s == "-v7.3")
                    throw MLabError("save: binary .mat formats are not supported");
                if (!s.empty() && s.front() == '-')
                    throw MLabError("save: unsupported flag '" + s + "'");
                varnames.push_back(s);
            }
            (void)asciiFlag; // currently the only supported format

            if (varnames.empty())
                throw MLabError("save: at least one variable name is required");

            std::ostringstream out;
            for (size_t vi = 0; vi < varnames.size(); ++vi) {
                MValue *v = ctx.env->get(varnames[vi]);
                if (!v)
                    throw MLabError("save: variable '" + varnames[vi] + "' not found");
                if (v->type() != MType::DOUBLE)
                    throw MLabError("save: only numeric (double) variables supported in ascii mode");
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
                throw MLabError(std::string("save: ") + e.what());
            }
        });

    engine.registerFunction(
        "load",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty() || !args[0].isChar())
                throw MLabError("load: filename required");
            std::string filename = args[0].toString();

            // Ignore -ascii flag; we only support ascii anyway.
            for (size_t i = 1; i < args.size(); ++i) {
                if (!args[i].isChar()) continue;
                std::string s = args[i].toString();
                if (s == "-mat" || s == "-v7" || s == "-v7.3")
                    throw MLabError("load: binary .mat formats are not supported");
                if (s == "-ascii") continue;
                if (!s.empty() && s.front() == '-')
                    throw MLabError("load: unsupported flag '" + s + "'");
            }

            auto resolved = ctx.engine->resolvePath(filename);
            std::string content;
            try {
                content = resolved.fs->readFile(resolved.path);
            } catch (const std::exception &e) {
                throw MLabError(std::string("load: ") + e.what());
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
                        throw MLabError("load: parse error near '" + line.substr(q) + "'");
                    row.push_back(v);
                    q = static_cast<size_t>(endp - line.c_str());
                }
                rows.push_back(std::move(row));
            }

            if (rows.empty())
                throw MLabError("load: no numeric data found");
            size_t cols = rows[0].size();
            for (auto &r : rows) {
                if (r.size() != cols)
                    throw MLabError("load: inconsistent column count across rows");
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
                throw MLabError("load: cannot derive a valid variable name from filename");
            ctx.env->set(stem, std::move(M));
        });

    engine.registerFunction(
        "fopen",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty() || !args[0].isChar())
                throw MLabError("fopen: filename must be a char array");

            // `fopen('all')` — only as the sole argument — returns a row
            // vector of every user-opened fid. MATLAB-compatible and
            // useful for diagnosing leaks. With a mode argument, 'all'
            // falls through to being a literal filename.
            if (args.size() == 1 && args[0].toString() == "all") {
                auto ids = ctx.engine->openFileIds();
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
            int fid = ctx.engine->openFile(path, mode);
            outs[0] = MValue::scalar(static_cast<double>(fid), alloc);
            // MATLAB: `[fid, errmsg] = fopen(...)` — errmsg is the reason
            // for a failed open, empty string on success.
            if (nargout > 1) {
                outs[1] = MValue::fromString(
                    fid < 0 ? ctx.engine->lastFopenError() : std::string(), alloc);
            }
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
                f->lastError = "End of file reached.";
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

    // ── ferror ──────────────────────────────────────────────────
    //
    //   msg           = ferror(fid)
    //   [msg, errnum] = ferror(fid)
    //   msg           = ferror(fid, 'clear')   — resets the error state
    //
    // Returns the most recent soft-error on that fid (EOF from a read,
    // short match on fscanf, etc.). Empty string when no error. errnum
    // is 0/-1 to signal clean/error per MATLAB convention. Hard errors
    // (invalid fid, bad precision, write-on-read-fid) still throw
    // MLabError — they are programmer errors, not recoverable I/O faults.

    engine.registerFunction(
        "ferror",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty() || !args[0].isScalar())
                throw MLabError("ferror: file identifier required");
            int fid = static_cast<int>(args[0].toScalar());
            auto *f = ctx.engine->findFile(fid);
            if (!f)
                throw MLabError("ferror: invalid file identifier");

            bool clear = args.size() >= 2 && args[1].isChar() && args[1].toString() == "clear";
            std::string msg = f->lastError;
            if (clear)
                f->lastError.clear();

            outs[0] = MValue::fromString(msg, alloc);
            if (nargout > 1)
                outs[1] = MValue::scalar(msg.empty() ? 0.0 : -1.0, alloc);
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

    // Parse the `size` argument for fread / fscanf. Accepts:
    //   scalar N        → flat column of up to N values   (Flat mode)
    //   Inf             → flat column of all remaining    (Flat, limit=SIZE_MAX)
    //   [m n] row       → m×n matrix, column-major        (Matrix mode)
    //   [m Inf]         → m rows, as many columns as fit  (Matrix, cols=SIZE_MAX)
    // `limit` is the element cap for reading; `rows`/`cols` are only
    // consulted in Matrix mode. cols==SIZE_MAX means "open-ended" (Inf).
    struct SizeSpec
    {
        enum class Kind { Flat, Matrix };
        Kind kind;
        size_t limit;
        size_t rows;  // Matrix only
        size_t cols;  // Matrix only
        bool matrix() const { return kind == Kind::Matrix; }
    };
    auto parseReadSize = [](const MValue &sz, const char *fn) -> SizeSpec {
        if (sz.isScalar()) {
            double s = sz.toScalar();
            if (std::isinf(s))
                return SizeSpec{SizeSpec::Kind::Flat, SIZE_MAX, 0, 0};
            if (s < 0 || !std::isfinite(s))
                throw MLabError(std::string(fn) + ": size must be Inf or a non-negative integer");
            return SizeSpec{SizeSpec::Kind::Flat, static_cast<size_t>(s), 0, 0};
        }
        if (sz.numel() == 2) {
            double r = sz(0);
            double c = sz(1);
            if (r < 0 || !std::isfinite(r) || std::isinf(r))
                throw MLabError(std::string(fn) + ": rows in [m n] must be finite non-negative");
            size_t rows = static_cast<size_t>(r);
            size_t cols;
            size_t limit;
            if (std::isinf(c)) {
                cols = SIZE_MAX;
                limit = SIZE_MAX;
            } else {
                if (c < 0 || !std::isfinite(c))
                    throw MLabError(std::string(fn) + ": cols in [m n] must be finite non-negative or Inf");
                cols = static_cast<size_t>(c);
                limit = rows * cols;
            }
            return SizeSpec{SizeSpec::Kind::Matrix, limit, rows, cols};
        }
        throw MLabError(std::string(fn) + ": size must be scalar, Inf, or a 2-element vector");
    };

    // Shape the flat `values` vector into either a column (Flat mode) or
    // an m×n matrix (Matrix mode). For matrix mode, unread cells stay
    // zero-initialised.
    auto shapeFreadOutput = [](std::vector<double> &&values, SizeSpec sz, Allocator *alloc) -> MValue {
        size_t n = values.size();
        if (!sz.matrix()) {
            if (n == 0)
                return MValue::matrix(0, 0, MType::DOUBLE, alloc);
            MValue M = MValue::matrix(n, 1, MType::DOUBLE, alloc);
            std::memcpy(M.doubleDataMut(), values.data(), n * sizeof(double));
            return M;
        }
        // Matrix mode: cols follow n when Inf, fixed otherwise.
        size_t cols_out = (sz.cols == SIZE_MAX)
                             ? (sz.rows == 0 ? 0 : (n + sz.rows - 1) / sz.rows)
                             : sz.cols;
        if (sz.rows == 0 || cols_out == 0)
            return MValue::matrix(sz.rows, cols_out, MType::DOUBLE, alloc);
        MValue M = MValue::matrix(sz.rows, cols_out, MType::DOUBLE, alloc);
        std::memcpy(M.doubleDataMut(), values.data(), n * sizeof(double));
        return M;
    };

    // Parse the machineformat argument of fread / fwrite. Returns true
    // for big-endian, false for little-endian. We treat 'native' as LE
    // because every target we support (x86_64, ARM64, WASM) is LE.
    auto parseEndian = [](const std::string &raw, const char *fn) -> bool {
        std::string lo;
        lo.reserve(raw.size());
        for (char c : raw) lo += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lo == "b" || lo == "ieee-be" || lo == "ieee-be.l64") return true;
        if (lo == "l" || lo == "ieee-le" || lo == "ieee-le.l64" ||
            lo == "n" || lo == "native")
            return false;
        throw MLabError(std::string(fn) + ": unsupported machine format '" + raw + "'");
    };
    auto byteSwap = [](char *p, size_t n) {
        for (size_t i = 0, j = n - 1; i < j; ++i, --j) std::swap(p[i], p[j]);
    };

    engine.registerFunction(
        "fread",
        [parsePrecision, parseReadSize, shapeFreadOutput, parseEndian, byteSwap](
            Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty() || !args[0].isScalar())
                throw MLabError("fread: file identifier required");
            int fid = static_cast<int>(args[0].toScalar());
            auto *f = ctx.engine->findFile(fid);
            if (!f || !f->forRead)
                throw MLabError("fread: invalid file identifier");

            SizeSpec sz{SizeSpec::Kind::Flat, SIZE_MAX, 0, 0};
            if (args.size() >= 2)
                sz = parseReadSize(args[1], "fread");

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

            // Optional machineformat argument — big-endian support.
            bool be = false;
            if (args.size() >= 4) {
                if (!args[3].isChar())
                    throw MLabError("fread: machine format must be a char array");
                be = parseEndian(args[3].toString(), "fread");
            }

            size_t available = f->buffer.size() - f->cursor;
            size_t maxElems = available / bsize;
            size_t n = std::min(sz.limit, maxElems);
            if (sz.limit != SIZE_MAX && n < sz.limit)
                f->lastError = "End of file reached.";

            std::vector<double> values(n);
            for (size_t i = 0; i < n; ++i) {
                // Copy into a local buffer so we can byte-swap safely
                // without mutating f->buffer.
                char tmp[8];
                std::memcpy(tmp, f->buffer.data() + f->cursor + i * bsize, bsize);
                if (be && bsize > 1) byteSwap(tmp, bsize);

                double v = 0.0;
                if (kind == 0) { // unsigned
                    switch (bsize) {
                    case 1: { uint8_t  x; std::memcpy(&x, tmp, 1); v = x; break; }
                    case 2: { uint16_t x; std::memcpy(&x, tmp, 2); v = x; break; }
                    case 4: { uint32_t x; std::memcpy(&x, tmp, 4); v = x; break; }
                    case 8: { uint64_t x; std::memcpy(&x, tmp, 8); v = static_cast<double>(x); break; }
                    }
                } else if (kind == 1) { // signed
                    switch (bsize) {
                    case 1: { int8_t  x; std::memcpy(&x, tmp, 1); v = x; break; }
                    case 2: { int16_t x; std::memcpy(&x, tmp, 2); v = x; break; }
                    case 4: { int32_t x; std::memcpy(&x, tmp, 4); v = x; break; }
                    case 8: { int64_t x; std::memcpy(&x, tmp, 8); v = static_cast<double>(x); break; }
                    }
                } else { // float
                    if (bsize == 4) { float x; std::memcpy(&x, tmp, 4); v = x; }
                    else            {                     std::memcpy(&v, tmp, 8); }
                }
                values[i] = v;
            }
            f->cursor += n * bsize;
            outs[0] = shapeFreadOutput(std::move(values), sz, alloc);
            if (nargout > 1)
                outs[1] = MValue::scalar(static_cast<double>(n), alloc);
        });

    // ── fscanf / sscanf ─────────────────────────────────────────
    //
    // Formatted reading. The format string is applied CYCLICALLY to the
    // input until either (a) the input runs out / a match fails or (b)
    // `size` elements have been produced. The format is a scanf-style
    // subset:
    //
    //   %d  %i  %u        decimal integer
    //   %f  %e  %g        float / scientific / general
    //   %x  %X            hex
    //   %o                octal
    //   %s                whitespace-delimited token (skips leading ws)
    //   %c                exactly N characters (default 1, no ws skip)
    //   %*…               suppress (read but don't emit)
    //   width digits      max chars for %s/%c; ignored for numeric
    //
    // Output type follows MATLAB's rule: if the format contains ANY
    // numeric conversion, the result is a column vector of doubles
    // (where %s/%c characters become ASCII codes). If the format has
    // only text conversions, the result is a char array with the
    // characters concatenated in order.
    //
    // Not supported: [set]-ranges, fscanf matrix-shape size=[m n]
    // (column-only for now).

    struct ScanfOut
    {
        std::vector<double> values;
        size_t count;
        size_t bytesConsumed;
    };

    // Does the format contain any non-text conversion? Controls output
    // type. Text-only → char array; anything else → double column.
    auto formatHasNumeric = [](const std::string &fmt) -> bool {
        for (size_t i = 0; i < fmt.size(); ++i) {
            if (fmt[i] != '%') continue;
            ++i;
            if (i < fmt.size() && fmt[i] == '*') ++i;
            while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) ++i;
            if (i >= fmt.size()) break;
            char spec = fmt[i];
            if (spec == 'd' || spec == 'i' || spec == 'u' ||
                spec == 'f' || spec == 'e' || spec == 'g' ||
                spec == 'E' || spec == 'G' ||
                spec == 'x' || spec == 'X' || spec == 'o')
                return true;
        }
        return false;
    };

    auto scanfCycle = [](const std::string &input, const std::string &fmt,
                         size_t limit) -> ScanfOut {
        std::vector<double> out;
        size_t inPos = 0;
        size_t count = 0;

        while (count < limit && inPos < input.size()) {
            size_t beforeIn = inPos;
            size_t beforeCount = count;
            size_t fmtPos = 0;
            bool ok = true;

            while (fmtPos < fmt.size() && ok) {
                char fc = fmt[fmtPos];
                if (std::isspace(static_cast<unsigned char>(fc))) {
                    while (inPos < input.size() &&
                           std::isspace(static_cast<unsigned char>(input[inPos])))
                        ++inPos;
                    ++fmtPos;
                    continue;
                }
                if (fc != '%') {
                    if (inPos >= input.size() || input[inPos] != fc) { ok = false; break; }
                    ++inPos;
                    ++fmtPos;
                    continue;
                }
                ++fmtPos;
                if (fmtPos >= fmt.size()) { ok = false; break; }

                bool suppress = false;
                if (fmt[fmtPos] == '*') { suppress = true; ++fmtPos; }
                int width = -1;
                while (fmtPos < fmt.size() &&
                       std::isdigit(static_cast<unsigned char>(fmt[fmtPos]))) {
                    if (width < 0) width = 0;
                    width = width * 10 + (fmt[fmtPos] - '0');
                    ++fmtPos;
                }
                if (fmtPos >= fmt.size()) { ok = false; break; }

                // %[set] is a char-class conversion — peek instead of
                // consuming so we can parse the full [abc^-] body.
                if (fmt[fmtPos] == '[') {
                    ++fmtPos; // past '['
                    bool negate = false;
                    if (fmtPos < fmt.size() && fmt[fmtPos] == '^') {
                        negate = true;
                        ++fmtPos;
                    }
                    std::array<bool, 256> member{};
                    bool first = true;
                    while (fmtPos < fmt.size() && (first || fmt[fmtPos] != ']')) {
                        first = false;
                        char c = fmt[fmtPos];
                        if (fmtPos + 2 < fmt.size() && fmt[fmtPos + 1] == '-'
                            && fmt[fmtPos + 2] != ']') {
                            unsigned lo = static_cast<unsigned char>(c);
                            unsigned hi = static_cast<unsigned char>(fmt[fmtPos + 2]);
                            if (lo > hi) std::swap(lo, hi);
                            for (unsigned ch = lo; ch <= hi; ++ch)
                                member[ch] = true;
                            fmtPos += 3;
                        } else {
                            member[static_cast<unsigned char>(c)] = true;
                            ++fmtPos;
                        }
                    }
                    if (fmtPos >= fmt.size()) { ok = false; break; }
                    ++fmtPos; // past closing ']'

                    // %[set] does NOT skip leading whitespace (like %c).
                    if (inPos >= input.size()) { ok = false; break; }
                    size_t tokenStart = inPos;
                    size_t maxEnd = (width < 0)
                                        ? input.size()
                                        : std::min(inPos + static_cast<size_t>(width),
                                                   input.size());
                    while (inPos < maxEnd) {
                        bool m = member[static_cast<unsigned char>(input[inPos])];
                        if (negate ? m : !m) break;
                        ++inPos;
                    }
                    if (inPos == tokenStart) { ok = false; break; }
                    if (!suppress) {
                        for (size_t p = tokenStart; p < inPos && count < limit; ++p) {
                            out.push_back(static_cast<double>(
                                static_cast<unsigned char>(input[p])));
                            ++count;
                        }
                    }
                    continue;
                }

                char spec = fmt[fmtPos++];

                // Numeric + %s skip leading whitespace. %c does NOT —
                // it matches literal characters including whitespace.
                if (spec != 'c') {
                    while (inPos < input.size() &&
                           std::isspace(static_cast<unsigned char>(input[inPos])))
                        ++inPos;
                }
                if (inPos >= input.size()) { ok = false; break; }

                if (spec == 's') {
                    size_t tokenStart = inPos;
                    size_t maxEnd = (width < 0)
                                        ? input.size()
                                        : std::min(inPos + static_cast<size_t>(width),
                                                   input.size());
                    while (inPos < maxEnd &&
                           !std::isspace(static_cast<unsigned char>(input[inPos])))
                        ++inPos;
                    if (inPos == tokenStart) { ok = false; break; }
                    if (!suppress) {
                        for (size_t p = tokenStart; p < inPos && count < limit; ++p) {
                            out.push_back(static_cast<double>(
                                static_cast<unsigned char>(input[p])));
                            ++count;
                        }
                    }
                    continue;
                }

                if (spec == 'c') {
                    size_t n = (width < 0) ? 1u : static_cast<size_t>(width);
                    if (inPos + n > input.size()) { ok = false; break; }
                    if (!suppress) {
                        for (size_t p = 0; p < n && count < limit; ++p) {
                            out.push_back(static_cast<double>(
                                static_cast<unsigned char>(input[inPos + p])));
                            ++count;
                        }
                    }
                    inPos += n;
                    continue;
                }

                const char *start = input.c_str() + inPos;
                char *endp = nullptr;
                double v = 0.0;

                switch (spec) {
                case 'd': case 'i': {
                    long long iv = std::strtoll(start, &endp, 10);
                    if (endp == start) { ok = false; break; }
                    v = static_cast<double>(iv);
                    break;
                }
                case 'u': {
                    unsigned long long uv = std::strtoull(start, &endp, 10);
                    if (endp == start) { ok = false; break; }
                    v = static_cast<double>(uv);
                    break;
                }
                case 'f': case 'e': case 'g': case 'E': case 'G': {
                    v = std::strtod(start, &endp);
                    if (endp == start) { ok = false; break; }
                    break;
                }
                case 'x': case 'X': {
                    unsigned long long uv = std::strtoull(start, &endp, 16);
                    if (endp == start) { ok = false; break; }
                    v = static_cast<double>(uv);
                    break;
                }
                case 'o': {
                    unsigned long long uv = std::strtoull(start, &endp, 8);
                    if (endp == start) { ok = false; break; }
                    v = static_cast<double>(uv);
                    break;
                }
                default:
                    throw MLabError(std::string("scanf: unsupported conversion '%")
                                    + spec + "'");
                }
                if (!ok) break;

                inPos += static_cast<size_t>(endp - start);
                if (!suppress) {
                    out.push_back(v);
                    if (++count >= limit) break;
                }
            }

            // No progress this cycle → bail. Roll back any partially-
            // consumed input so ftell/fscanf can retry from a clean
            // boundary if the caller wants to.
            if (count == beforeCount) {
                inPos = beforeIn;
                break;
            }
        }
        return ScanfOut{std::move(out), count, inPos};
    };

    auto makeColumn = [](std::vector<double> &&vals, Allocator *alloc) -> MValue {
        if (vals.empty())
            return MValue::matrix(0, 0, MType::DOUBLE, alloc);
        auto M = MValue::matrix(vals.size(), 1, MType::DOUBLE, alloc);
        double *data = M.doubleDataMut();
        std::memcpy(data, vals.data(), vals.size() * sizeof(double));
        return M;
    };

    auto makeCharRow = [](const std::vector<double> &vals, Allocator *alloc) -> MValue {
        std::string s;
        s.reserve(vals.size());
        for (double v : vals)
            s.push_back(static_cast<char>(static_cast<int>(v)));
        return MValue::fromString(s, alloc);
    };

    // Column-major char matrix from the flat `vals` vector. Unfilled
    // cells stay zero (MATLAB's documented fill for partial char reads).
    auto makeCharMatrix = [](const std::vector<double> &vals, SizeSpec sz,
                             Allocator *alloc) -> MValue {
        size_t n = vals.size();
        size_t cols_out = (sz.cols == SIZE_MAX)
                             ? (sz.rows == 0 ? 0 : (n + sz.rows - 1) / sz.rows)
                             : sz.cols;
        if (sz.rows == 0 || cols_out == 0)
            return MValue::matrix(sz.rows, cols_out, MType::CHAR, alloc);
        MValue M = MValue::matrix(sz.rows, cols_out, MType::CHAR, alloc);
        char *data = M.charDataMut();
        for (size_t i = 0; i < n; ++i)
            data[i] = static_cast<char>(static_cast<int>(vals[i]));
        return M;
    };

    // Chooses the output shape per the MATLAB contract: char array when
    // the format has only %s/%c conversions, column-of-doubles otherwise.
    // Takes a pre-computed `hasNumericConv` flag so the caller doesn't
    // need to re-walk the format string.
    auto shapeScanfOutput = [makeColumn, makeCharRow](
                                std::vector<double> &&vals, bool hasNumericConv,
                                Allocator *alloc) -> MValue {
        if (hasNumericConv)
            return makeColumn(std::move(vals), alloc);
        return makeCharRow(vals, alloc);
    };

    engine.registerFunction(
        "fscanf",
        [scanfCycle, shapeScanfOutput, parseReadSize, shapeFreadOutput, formatHasNumeric,
         makeCharMatrix](
            Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() < 2 || !args[0].isScalar() || !args[1].isChar())
                throw MLabError("fscanf: requires (fid, format [, size])");
            int fid = static_cast<int>(args[0].toScalar());
            auto *f = ctx.engine->findFile(fid);
            if (!f || !f->forRead)
                throw MLabError("fscanf: invalid file identifier");

            SizeSpec sz{SizeSpec::Kind::Flat, SIZE_MAX, 0, 0};
            if (args.size() >= 3)
                sz = parseReadSize(args[2], "fscanf");

            std::string input(f->buffer.begin() + f->cursor, f->buffer.end());
            std::string fmt = args[1].toString();
            auto r = scanfCycle(input, fmt, sz.limit);
            f->cursor += r.bytesConsumed;

            // Populate ferror on a partial match so scripts can distinguish
            // "input exhausted" from "fewer items matched than requested".
            if (f->cursor >= f->buffer.size())
                f->lastError = "End of file reached.";
            else if (sz.limit != SIZE_MAX && r.count < sz.limit)
                f->lastError = "Matching failure.";

            // Matrix-shape dispatch: numeric format → double matrix,
            // pure-text format → char matrix. Flat size keeps the
            // per-format column-or-row shape from shapeScanfOutput.
            const bool hasNum = formatHasNumeric(fmt);
            if (sz.matrix() && hasNum)
                outs[0] = shapeFreadOutput(std::move(r.values), sz, alloc);
            else if (sz.matrix())
                outs[0] = makeCharMatrix(r.values, sz, alloc);
            else
                outs[0] = shapeScanfOutput(std::move(r.values), hasNum, alloc);
            if (nargout > 1)
                outs[1] = MValue::scalar(static_cast<double>(r.count), alloc);
        });

    engine.registerFunction(
        "sscanf",
        [scanfCycle, shapeScanfOutput, parseReadSize, shapeFreadOutput, formatHasNumeric,
         makeCharMatrix](
            Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() < 2 || !args[0].isChar() || !args[1].isChar())
                throw MLabError("sscanf: requires (str, format [, size])");

            SizeSpec sz{SizeSpec::Kind::Flat, SIZE_MAX, 0, 0};
            if (args.size() >= 3)
                sz = parseReadSize(args[2], "sscanf");

            std::string fmt = args[1].toString();
            auto r = scanfCycle(args[0].toString(), fmt, sz.limit);

            const bool hasNum = formatHasNumeric(fmt);
            if (sz.matrix() && hasNum)
                outs[0] = shapeFreadOutput(std::move(r.values), sz, alloc);
            else if (sz.matrix())
                outs[0] = makeCharMatrix(r.values, sz, alloc);
            else
                outs[0] = shapeScanfOutput(std::move(r.values), hasNum, alloc);
            if (nargout > 1)
                outs[1] = MValue::scalar(static_cast<double>(r.count), alloc);
            if (nargout > 2)
                outs[2] = MValue::fromString("", alloc); // errmsg — always empty for now
            if (nargout > 3)
                outs[3] = MValue::scalar(static_cast<double>(r.bytesConsumed + 1), alloc);
        });

    // ── textscan ────────────────────────────────────────────────
    //
    // Tabular/columnar reading, MATLAB-compatible:
    //
    //   C = textscan(fid, formatSpec)
    //   C = textscan(fid, formatSpec, N)
    //   C = textscan(str, formatSpec, …)
    //   C = textscan(…, 'Delimiter', d, 'EndOfLine', e, 'HeaderLines', k)
    //
    // Unlike fscanf, textscan returns a CELL ARRAY with one cell per
    // conversion in formatSpec — each cell a column vector of that
    // conversion's values. Numeric specs (%d %i %u %f %e %g %x %o)
    // produce a column of doubles; %s produces a column cell of
    // strings (cellstr). N caps the number of full format cycles.
    //
    // Supported options:
    //   'Delimiter'   char array, or cell array of char arrays. Default
    //                 is whitespace (space + tab).
    //   'EndOfLine'   char array of characters that terminate a line
    //                 (and thus also a token). Default "\n". Always
    //                 merged into the effective token-boundary set so
    //                 multi-line CSV doesn't glue values across rows.
    //   'HeaderLines' integer, number of leading lines to skip.
    //
    // Not yet: CommentStyle, TreatAsEmpty, EmptyValue,
    // MultipleDelimsAsOne, %c, %[set]. Unknown names throw.

    struct TextscanOptions
    {
        // Empty → whitespace-default mode (runs of space/tab/\f/\v + EOL
        // count as a single field boundary). Non-empty → explicit-delim
        // mode: ONE char in this set is one field boundary, and EOL
        // chars are still separate record boundaries that always
        // collapse.
        std::string delimiters;
        std::string endOfLine = "\n";
        size_t headerLines = 0;
        // Line-comment marker. When non-empty, anything from its first
        // occurrence up to the next EndOfLine char is ignored.
        std::string commentStyle;
        // Tokens whose text equals any entry in this set are coerced to
        // "empty" (numeric → EmptyValue, %s → '').
        std::vector<std::string> treatAsEmpty;
        // In explicit-delim mode only: when true, runs of delim chars
        // collapse to one boundary (no empty fields emitted). Default
        // false, matching MATLAB; whitespace-default mode always
        // collapses regardless.
        bool multipleDelimsAsOne = false;
        // Value substituted for empty numeric fields. MATLAB default NaN.
        double emptyValue = std::numeric_limits<double>::quiet_NaN();
    };

    // Parse formatSpec into an ordered conversion list. Unrecognised
    // % codes throw before any scanning happens.
    struct TextscanConv { char spec; bool suppress; int width; };
    auto parseTextscanFormat = [](const std::string &fmt) -> std::vector<TextscanConv> {
        std::vector<TextscanConv> out;
        for (size_t i = 0; i < fmt.size(); ++i) {
            if (fmt[i] != '%')
                continue;                     // literal chars between specs are ignored
            ++i;
            bool suppress = false;
            if (i < fmt.size() && fmt[i] == '*') { suppress = true; ++i; }
            int width = -1;
            while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) {
                if (width < 0) width = 0;
                width = width * 10 + (fmt[i] - '0');
                ++i;
            }
            if (i >= fmt.size())
                throw MLabError("textscan: truncated format specifier");
            char spec = fmt[i];
            switch (spec) {
            case 'd': case 'i': case 'u':
            case 'f': case 'e': case 'g': case 'E': case 'G':
            case 'x': case 'X': case 'o':
            case 's':
                break;
            default:
                throw MLabError(std::string("textscan: unsupported conversion '%")
                                + spec + "'");
            }
            out.push_back({spec, suppress, width});
        }
        if (out.empty())
            throw MLabError("textscan: format must contain at least one conversion");
        return out;
    };

    engine.registerFunction(
        "textscan",
        [parseTextscanFormat](Span<const MValue> args, size_t nargout, Span<MValue> outs,
                              CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() < 2 || !args[1].isChar())
                throw MLabError("textscan: requires (source, format [, N] [, opt, value …])");

            // Source — fid scalar or char array.
            std::string input;
            Engine::OpenFile *srcFile = nullptr;
            if (args[0].isChar()) {
                input = args[0].toString();
            } else if (args[0].isScalar()) {
                int fid = static_cast<int>(args[0].toScalar());
                srcFile = ctx.engine->findFile(fid);
                if (!srcFile || !srcFile->forRead)
                    throw MLabError("textscan: invalid file identifier");
                input.assign(srcFile->buffer.begin() + srcFile->cursor,
                             srcFile->buffer.end());
            } else {
                throw MLabError("textscan: source must be a file identifier or char array");
            }

            std::string fmt = args[1].toString();
            auto convs = parseTextscanFormat(fmt);

            // Optional positional N, then name-value pairs.
            size_t argIdx = 2;
            size_t cycleCap = SIZE_MAX;
            if (argIdx < args.size() && args[argIdx].isScalar() && !args[argIdx].isChar()) {
                double d = args[argIdx].toScalar();
                if (!std::isinf(d)) {
                    if (d < 0 || !std::isfinite(d))
                        throw MLabError("textscan: N must be Inf or a non-negative integer");
                    cycleCap = static_cast<size_t>(d);
                }
                ++argIdx;
            }

            // Parse name/value options into a TextscanOptions struct.
            TextscanOptions opts;
            auto lower = [](std::string s) {
                for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                return s;
            };
            while (argIdx + 1 < args.size()) {
                if (!args[argIdx].isChar())
                    throw MLabError("textscan: option name must be a char array");
                std::string name = lower(args[argIdx].toString());
                const MValue &val = args[argIdx + 1];
                if (name == "delimiter") {
                    if (val.isChar()) {
                        opts.delimiters = val.toString();
                    } else if (val.isCell()) {
                        // Concatenate every char in the cells into one delimiter set.
                        opts.delimiters.clear();
                        for (size_t i = 0; i < val.numel(); ++i) {
                            const MValue &d = val.cellAt(i);
                            if (d.isChar()) opts.delimiters += d.toString();
                        }
                    } else {
                        throw MLabError("textscan: 'Delimiter' must be a char array or cell");
                    }
                } else if (name == "endofline") {
                    if (!val.isChar())
                        throw MLabError("textscan: 'EndOfLine' must be a char array");
                    opts.endOfLine = val.toString();
                } else if (name == "headerlines") {
                    double d = val.toScalar();
                    if (d < 0 || !std::isfinite(d))
                        throw MLabError("textscan: 'HeaderLines' must be a non-negative integer");
                    opts.headerLines = static_cast<size_t>(d);
                } else if (name == "commentstyle") {
                    if (!val.isChar())
                        throw MLabError("textscan: 'CommentStyle' must be a char array");
                    opts.commentStyle = val.toString();
                } else if (name == "treatasempty") {
                    if (val.isChar()) {
                        opts.treatAsEmpty.push_back(val.toString());
                    } else if (val.isCell()) {
                        for (size_t i = 0; i < val.numel(); ++i) {
                            const MValue &e = val.cellAt(i);
                            if (e.isChar()) opts.treatAsEmpty.push_back(e.toString());
                        }
                    } else {
                        throw MLabError("textscan: 'TreatAsEmpty' must be a char array or cell");
                    }
                } else if (name == "multipledelimsasone") {
                    if (!val.isScalar() && !val.isLogical())
                        throw MLabError("textscan: 'MultipleDelimsAsOne' must be logical/numeric");
                    opts.multipleDelimsAsOne = (val.toScalar() != 0.0);
                } else if (name == "emptyvalue") {
                    if (!val.isScalar())
                        throw MLabError("textscan: 'EmptyValue' must be a numeric scalar");
                    opts.emptyValue = val.toScalar();
                } else {
                    throw MLabError("textscan: unsupported option '" + args[argIdx].toString()
                                    + "'");
                }
                argIdx += 2;
            }

            // Character classification. Whitespace-default mode (no
            // Delimiter supplied) treats whitespace as a collapsing
            // field separator; explicit-delim mode reserves whitespace
            // for trimming around fields instead.
            const bool useWsDefault = opts.delimiters.empty();
            size_t pos = 0;

            auto isEol = [&opts](char c) {
                return opts.endOfLine.find(c) != std::string::npos;
            };
            auto isWs = [](char c) {
                return c == ' ' || c == '\t' || c == '\f' || c == '\v';
            };
            auto isExplicitDelim = [&opts](char c) {
                return opts.delimiters.find(c) != std::string::npos;
            };
            auto atComment = [&opts, &input, &pos]() -> bool {
                return !opts.commentStyle.empty() &&
                       pos + opts.commentStyle.size() <= input.size() &&
                       input.compare(pos, opts.commentStyle.size(), opts.commentStyle) == 0;
            };
            auto isEmpty = [&opts](const std::string &tok) {
                for (auto &e : opts.treatAsEmpty)
                    if (tok == e) return true;
                return false;
            };

            // Skip header lines using the configured EndOfLine chars.
            for (size_t h = 0; h < opts.headerLines && pos < input.size(); ++h) {
                while (pos < input.size() && !isEol(input[pos])) ++pos;
                if (pos < input.size()) ++pos;  // consume the EOL char itself
            }

            // skipLeading: eat all whitespace / EOL / comment chars before
            // the very first field of the whole run. Called once, before
            // the cycle loop.
            auto skipLeading = [&]() {
                while (pos < input.size()) {
                    if (atComment()) {
                        while (pos < input.size() && !isEol(input[pos])) ++pos;
                        continue;
                    }
                    char c = input[pos];
                    if (isEol(c) || isWs(c)) { ++pos; continue; }
                    break;
                }
            };

            // consumeBoundary: eat exactly ONE field/record boundary
            // between two adjacent fields. Returns false when there is
            // no boundary to consume (EOF or unexpected input), which
            // signals the caller to end the cycle.
            //
            // Whitespace-default: runs of whitespace+EOL+comments all
            // collapse to one boundary — MATLAB's documented contract.
            // Explicit-delim: EOL still collapses, but a single delim
            // char counts as ONE boundary, so "1,,3" yields three
            // fields with the middle one empty (subject to
            // MultipleDelimsAsOne, which collapses runs of the
            // explicit delim back together).
            auto consumeBoundary = [&]() -> bool {
                if (useWsDefault) {
                    size_t before = pos;
                    while (pos < input.size()) {
                        if (atComment()) {
                            while (pos < input.size() && !isEol(input[pos])) ++pos;
                            continue;
                        }
                        char c = input[pos];
                        if (isWs(c) || isEol(c)) { ++pos; continue; }
                        break;
                    }
                    return pos > before;
                }

                // Explicit-delim mode. Trim leading whitespace around
                // the boundary (ws is always stripped from fields, not
                // treated as a delim itself).
                while (pos < input.size() && isWs(input[pos])) ++pos;
                if (atComment()) {
                    while (pos < input.size() && !isEol(input[pos])) ++pos;
                }
                if (pos >= input.size()) return false;

                char c = input[pos];
                if (isEol(c)) {
                    // EOL runs always collapse, even in explicit-delim mode.
                    while (pos < input.size() && isEol(input[pos])) ++pos;
                    while (pos < input.size() && isWs(input[pos])) ++pos;
                    return true;
                }
                if (!isExplicitDelim(c)) return false;

                ++pos;  // consume exactly one delim
                if (opts.multipleDelimsAsOne) {
                    while (pos < input.size() && isExplicitDelim(input[pos])) ++pos;
                }
                while (pos < input.size() && isWs(input[pos])) ++pos;
                return true;
            };

            // readField: consume up to the next field/record boundary.
            // In explicit-delim mode, trailing whitespace inside the
            // field is trimmed (so "1 , 2" with delim ',' yields "1"
            // and "2", not "1 " and " 2").
            auto readField = [&]() -> std::string {
                size_t start = pos;
                while (pos < input.size()) {
                    char c = input[pos];
                    if (atComment()) break;
                    if (isEol(c)) break;
                    if (useWsDefault) {
                        if (isWs(c)) break;
                    } else {
                        if (isExplicitDelim(c)) break;
                    }
                    ++pos;
                }
                size_t end = pos;
                if (!useWsDefault) {
                    while (end > start && isWs(input[end - 1])) --end;
                }
                return input.substr(start, end - start);
            };

            std::vector<std::vector<double>> numCols(convs.size());
            std::vector<std::vector<std::string>> strCols(convs.size());
            size_t cycles = 0;

            skipLeading();                         // past leading ws/EOL/comments once
            bool atFirstField = true;              // no boundary-consume before the very first field

            while (cycles < cycleCap) {
                if (pos >= input.size()) break;

                size_t savedPos = pos;
                bool savedFirst = atFirstField;
                std::vector<std::pair<size_t, double>> stageNum;
                std::vector<std::pair<size_t, std::string>> stageStr;
                bool fullCycle = true;

                for (size_t i = 0; i < convs.size(); ++i) {
                    if (!atFirstField) {
                        if (!consumeBoundary()) { fullCycle = false; break; }
                    }
                    atFirstField = false;

                    // Note: we DO NOT bail on pos >= input.size() here.
                    // In explicit-delim mode a trailing separator creates
                    // a legitimate final empty field ("a,b," → 3 fields),
                    // and readField returning "" below gets routed into
                    // the empty-field branch correctly. In whitespace-
                    // default mode, the same empty-tok path distinguishes
                    // "no more data" and triggers fullCycle = false.
                    std::string tok = readField();

                    // Two kinds of "empty token" can flow out of readField:
                    //
                    //   1. The raw token was non-empty but matches one of
                    //      the TreatAsEmpty markers → coerce to empty,
                    //      treat as an explicitly empty field.
                    //   2. readField genuinely returned "" because we hit
                    //      EOL/EOF → partial cycle in ws-default mode, or
                    //      a real empty field between two explicit delims.
                    //
                    // The coercedEmpty flag keeps these apart so a
                    // TreatAsEmpty hit in whitespace-default mode still
                    // yields EmptyValue instead of rolling back.
                    bool coercedEmpty = !tok.empty() && isEmpty(tok);
                    if (coercedEmpty) tok.clear();

                    const TextscanConv &c = convs[i];
                    if (tok.empty()) {
                        if (!coercedEmpty && useWsDefault) {
                            fullCycle = false;
                            break;
                        }
                        if (c.suppress) continue;
                        if (c.spec == 's')
                            stageStr.push_back({i, std::string()});
                        else
                            stageNum.push_back({i, opts.emptyValue});
                        continue;
                    }

                    if (c.width > 0 && static_cast<int>(tok.size()) > c.width)
                        tok.resize(static_cast<size_t>(c.width));

                    if (c.spec == 's') {
                        if (!c.suppress) stageStr.push_back({i, std::move(tok)});
                        continue;
                    }

                    const char *start = tok.c_str();
                    char *endp = nullptr;
                    double v = 0.0;
                    switch (c.spec) {
                    case 'd': case 'i': {
                        long long iv = std::strtoll(start, &endp, 10);
                        v = static_cast<double>(iv);
                        break;
                    }
                    case 'u': {
                        unsigned long long uv = std::strtoull(start, &endp, 10);
                        v = static_cast<double>(uv);
                        break;
                    }
                    case 'f': case 'e': case 'g': case 'E': case 'G':
                        v = std::strtod(start, &endp); break;
                    case 'x': case 'X': {
                        unsigned long long uv = std::strtoull(start, &endp, 16);
                        v = static_cast<double>(uv);
                        break;
                    }
                    case 'o': {
                        unsigned long long uv = std::strtoull(start, &endp, 8);
                        v = static_cast<double>(uv);
                        break;
                    }
                    }
                    if (endp == start) { fullCycle = false; break; }
                    if (!c.suppress) stageNum.push_back({i, v});
                }

                if (!fullCycle) {
                    pos = savedPos;
                    atFirstField = savedFirst;
                    break;
                }

                for (auto &p : stageNum) numCols[p.first].push_back(p.second);
                for (auto &p : stageStr) strCols[p.first].push_back(std::move(p.second));
                ++cycles;
            }

            if (srcFile)
                srcFile->cursor += pos;

            // Build the outer cell array — one slot per non-suppressed
            // conversion. Suppressed conversions contribute no column.
            size_t nonSup = 0;
            for (auto &c : convs) if (!c.suppress) ++nonSup;
            MValue result = MValue::cell(1, nonSup);
            size_t slot = 0;
            for (size_t i = 0; i < convs.size(); ++i) {
                if (convs[i].suppress) continue;
                if (convs[i].spec == 's') {
                    size_t k = strCols[i].size();
                    MValue inner = MValue::cell(k, 1);
                    for (size_t j = 0; j < k; ++j)
                        inner.cellAt(j) = MValue::fromString(strCols[i][j], alloc);
                    result.cellAt(slot++) = std::move(inner);
                } else {
                    size_t k = numCols[i].size();
                    MValue col = (k == 0)
                                     ? MValue::matrix(0, 0, MType::DOUBLE, alloc)
                                     : MValue::matrix(k, 1, MType::DOUBLE, alloc);
                    if (k > 0)
                        std::memcpy(col.doubleDataMut(), numCols[i].data(),
                                    k * sizeof(double));
                    result.cellAt(slot++) = std::move(col);
                }
            }
            outs[0] = std::move(result);
        });

    engine.registerFunction(
        "fwrite",
        [parsePrecision, parseEndian, byteSwap](
            Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() < 2 || !args[0].isScalar())
                throw MLabError("fwrite: requires (fid, array [, precision [, machineformat]])");
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

            bool be = false;
            if (args.size() >= 4) {
                if (!args[3].isChar())
                    throw MLabError("fwrite: machine format must be a char array");
                be = parseEndian(args[3].toString(), "fwrite");
            }

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
                if (be && bsize > 1)
                    byteSwap(dst, bsize);
                dst += bsize;
            }
            // Same cursor-based write contract as fprintf — see there
            // for the appendOnly rationale.
            size_t writePos = f->appendOnly ? f->buffer.size() : f->cursor;
            if (writePos + bytes.size() > f->buffer.size())
                f->buffer.resize(writePos + bytes.size());
            std::memcpy(f->buffer.data() + writePos, bytes.data(), bytes.size());
            f->cursor = writePos + bytes.size();
            outs[0] = MValue::scalar(static_cast<double>(numel), alloc);
        });
}

} // namespace mlab