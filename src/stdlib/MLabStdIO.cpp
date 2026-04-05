#include "MLabStdLibrary.hpp"

#include <cmath>
#include <iostream>
#include <sstream>

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

    engine.registerFunction("fprintf",
                            [mlab_sprintf](Span<const MValue> args,
                                           size_t nargout,
                                           Span<MValue> outs,
                                           CallContext &ctx) {
                                if (args.empty())
                                    return;
                                size_t fmtIdx = 0;
                                if (args.size() >= 2 && args[0].isScalar() && args[1].isChar())
                                    fmtIdx = 1;
                                if (!args[fmtIdx].isChar())
                                    return;
                                std::string result = mlab_sprintf(args[fmtIdx].toString(),
                                                                  args,
                                                                  fmtIdx + 1);
                                ctx.engine->outputText(result);
                                return;
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
}

} // namespace mlab