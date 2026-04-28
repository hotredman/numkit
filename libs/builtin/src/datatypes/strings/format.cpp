// libs/builtin/src/datatypes/strings/format.cpp

#include <numkit/builtin/datatypes/strings/format.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cctype>
#include <cstdio>
#include <sstream>
#include <vector>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

std::string formatOnce(const std::string &fmt, Span<const Value> args, size_t argStart)
{
    std::ostringstream out;
    size_t ai = argStart;

    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] == '\\' && i + 1 < fmt.size()) {
            char next = fmt[i + 1];
            if (next == 'n')  { out << '\n'; i++; continue; }
            if (next == 't')  { out << '\t'; i++; continue; }
            if (next == '\\') { out << '\\'; i++; continue; }
            if (next == '\'') { out << '\''; i++; continue; }
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
                    std::snprintf(buf, sizeof(buf), ispec.c_str(),
                                  static_cast<long long>(args[ai].toScalar()));
                    out << buf;
                }
                ai++;
            } else if (type == 'u') {
                if (ai < args.size()) {
                    char buf[64];
                    std::string uspec = spec.substr(0, spec.size() - 1) + "llu";
                    std::snprintf(buf, sizeof(buf), uspec.c_str(),
                                  static_cast<unsigned long long>(args[ai].toScalar()));
                    out << buf;
                }
                ai++;
            } else if (type == 'x' || type == 'X') {
                if (ai < args.size()) {
                    char buf[64];
                    std::string xspec = spec.substr(0, spec.size() - 1) + "ll" + type;
                    std::snprintf(buf, sizeof(buf), xspec.c_str(),
                                  static_cast<unsigned long long>(args[ai].toScalar()));
                    out << buf;
                }
                ai++;
            } else if (type == 'o') {
                if (ai < args.size()) {
                    char buf[64];
                    std::string ospec = spec.substr(0, spec.size() - 1) + "llo";
                    std::snprintf(buf, sizeof(buf), ospec.c_str(),
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
}

size_t countFormatSpecs(const std::string &fmt)
{
    size_t n = 0;
    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] != '%') continue;
        if (i + 1 < fmt.size() && fmt[i + 1] == '%') { ++i; continue; }
        ++i;
        while (i < fmt.size()
               && (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == '0' || fmt[i] == ' '
                   || fmt[i] == '#' || fmt[i] == '.'
                   || std::isdigit(static_cast<unsigned char>(fmt[i]))))
            ++i;
        while (i < fmt.size() && (fmt[i] == 'l' || fmt[i] == 'h'))
            ++i;
        if (i < fmt.size()) ++n;
    }
    return n;
}

std::string formatCyclic(Allocator &alloc, const std::string &fmt,
                         Span<const Value> args, size_t argStart)
{
    Allocator *p = &alloc;
    std::vector<Value> stream;
    stream.reserve(args.size() > argStart ? args.size() - argStart : 0);
    for (size_t i = argStart; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.isChar() || a.isScalar()) {
            stream.push_back(a);
            continue;
        }
        size_t n = a.numel();
        for (size_t j = 0; j < n; ++j) {
            double v;
            if (a.type() == ValueType::DOUBLE) v = a.doubleData()[j];
            else if (a.isLogical())        v = a.logicalData()[j] ? 1.0 : 0.0;
            else                           v = a(j);
            stream.push_back(Value::scalar(v, p));
        }
    }

    size_t nSpecs = countFormatSpecs(fmt);
    if (nSpecs == 0 || stream.size() <= nSpecs)
        return formatOnce(fmt, Span<const Value>{stream.data(), stream.size()}, 0);

    std::string out;
    size_t pos = 0;
    while (pos < stream.size()) {
        size_t end = std::min(pos + nSpecs, stream.size());
        out += formatOnce(fmt, Span<const Value>{stream.data() + pos, end - pos}, 0);
        pos = end;
    }
    return out;
}

Value sprintf(Allocator &alloc, const Value &fmt, Span<const Value> args)
{
    Allocator *p = &alloc;
    if (!fmt.isChar())
        return Value::fromString("", p);
    std::string result = formatCyclic(alloc, fmt.toString(), args, 0);
    return Value::fromString(result, p);
}

// ════════════════════════════════════════════════════════════════════════
// Adapter
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void sprintf_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    Allocator &alloc = ctx.engine->allocator();
    if (args.empty()) {
        outs[0] = Value::fromString("", &alloc);
        return;
    }
    Span<const Value> rest{args.data() + 1, args.size() - 1};
    outs[0] = sprintf(alloc, args[0], rest);
}

} // namespace detail

} // namespace numkit::builtin
