#include "MLabStdLibrary.hpp"

#include <cmath>
#include <iostream>
#include <sstream>

namespace mlab {

void StdLibrary::registerIOFunctions(Engine &engine)
{
    engine.registerFunction("disp",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
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
                                    engine.outputText(os.str());
                                }
                                return;
                            });

    // ── shared printf-style formatter ──────────────────────────
    // Supports: %d %i %f %e %g %s %c %% %u %x %o
    //           width, precision, flags (-, +, 0, space)
    //           \n \t \\ \'
    // Arguments are consumed cyclically (MATLAB behaviour).
    auto mlab_sprintf = [](const std::string &fmt,
                           Span<const MValue> args,
                           size_t argStart) -> std::string {
        std::ostringstream out;
        size_t ai = argStart; // current argument index

        for (size_t i = 0; i < fmt.size(); ++i) {
            // ── escape sequences ──
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

            // ── format specifier ──
            if (fmt[i] == '%') {
                if (i + 1 < fmt.size() && fmt[i + 1] == '%') {
                    out << '%';
                    i++;
                    continue;
                }

                // Collect the full specifier: %[flags][width][.prec]type
                size_t start = i;
                i++; // skip '%'

                // flags
                while (i < fmt.size()
                       && (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == '0' || fmt[i] == ' '
                           || fmt[i] == '#'))
                    i++;
                // width (may be '*')
                if (i < fmt.size() && fmt[i] == '*') {
                    i++;
                } else {
                    while (i < fmt.size() && fmt[i] >= '0' && fmt[i] <= '9')
                        i++;
                }
                // precision
                if (i < fmt.size() && fmt[i] == '.') {
                    i++;
                    if (i < fmt.size() && fmt[i] == '*') {
                        i++;
                    } else {
                        while (i < fmt.size() && fmt[i] >= '0' && fmt[i] <= '9')
                            i++;
                    }
                }
                // length modifiers (skip)
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
                        // Replace 'd'/'i' with lld for long long
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
                    // Unknown specifier — output as-is
                    out << spec;
                }
                continue;
            }

            // ── ordinary character ──
            out << fmt[i];
        }
        return out.str();
    };

    engine.registerFunction("fprintf",
                            [&engine, mlab_sprintf](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                if (args.empty())
                                    return;
                                size_t fmtIdx = 0;
                                // Skip optional file id (1 = stdout, 2 = stderr)
                                if (args.size() >= 2 && args[0].isScalar() && args[1].isChar())
                                    fmtIdx = 1;
                                if (!args[fmtIdx].isChar())
                                    return;
                                std::string result = mlab_sprintf(args[fmtIdx].toString(),
                                                                  args,
                                                                  fmtIdx + 1);
                                engine.outputText(result);
                                return;
                            });

    engine.registerFunction("sprintf",
                            [&engine, mlab_sprintf](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                auto *alloc = &engine.allocator();
                                if (args.empty() || !args[0].isChar())
                                    { outs[0] = MValue::fromString("", alloc); return; }
                                std::string result = mlab_sprintf(args[0].toString(), args, 1);
                                { outs[0] = MValue::fromString(result, alloc); return; }
                            });

    engine.registerFunction("error",
                            [](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                std::string msg = args.empty() ? "Error" : args[0].toString();
                                throw std::runtime_error(msg);
                            });

    engine.registerFunction("warning",
                            [](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                if (!args.empty() && args[0].isChar())
                                    std::cerr << "Warning: " << args[0].toString() << "\n";
                                return;
                            });
}

} // namespace mlab