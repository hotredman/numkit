// libs/builtin/src/datatypes/strings/format.cpp

#include <numkit/builtin/language/strings/format.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <cctype>
#include <cstdio>
#include <sstream>

namespace numkit::builtin {

namespace {

// Apply a %s conversion spec (flags / width / precision) to a string value,
// matching MATLAB/C printf semantics: precision caps the number of characters
// emitted, width right-justifies (or left-justifies with the '-' flag) by
// padding with spaces. Length modifiers (l/h) are ignored. Done manually
// rather than via snprintf to avoid passing user-controlled format strings.
std::string applyStringSpec(const std::string &spec, std::string sv)
{
    bool leftAlign = false;
    size_t k = 1;                       // skip '%'
    for (; k < spec.size(); ++k) {
        const char c = spec[k];
        if (c == '-') leftAlign = true;
        else if (c == '+' || c == ' ' || c == '0' || c == '#') { /* flag */ }
        else break;
    }
    size_t width = 0;
    for (; k < spec.size() && std::isdigit(static_cast<unsigned char>(spec[k])); ++k)
        width = width * 10 + static_cast<size_t>(spec[k] - '0');
    long precision = -1;
    if (k < spec.size() && spec[k] == '.') {
        ++k;
        precision = 0;
        for (; k < spec.size() && std::isdigit(static_cast<unsigned char>(spec[k])); ++k)
            precision = precision * 10 + (spec[k] - '0');
    }

    if (precision >= 0 && static_cast<size_t>(precision) < sv.size())
        sv.resize(static_cast<size_t>(precision));
    if (width > sv.size()) {
        const std::string pad(width - sv.size(), ' ');
        return leftAlign ? sv + pad : pad + sv;
    }
    return sv;
}

} // namespace

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
                // MATLAB %s accepts both char arrays and string scalars, and
                // honours width / precision in the spec (e.g. %5s, %-5s, %.1s).
                if (ai < args.size()
                    && (args[ai].isChar() || args[ai].isString()))
                    out << applyStringSpec(spec, args[ai].toString());
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

std::string formatCyclic(const std::string &fmt, Span<const Value> args, size_t argStart, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    ScratchArena scratch(mr);
    ScratchVec<Value> stream(&scratch);
    stream.reserve(args.size() > argStart ? args.size() - argStart : 0);
    for (size_t i = argStart; i < args.size(); ++i) {
        const Value &a = args[i];
        // A string array supplies one argument per element (MATLAB cycles
        // the format over them, like a numeric array). A char array is a
        // single atomic %s argument, so it is NOT expanded here.
        if (a.isString()) {
            size_t n = a.numel();
            for (size_t j = 0; j < n; ++j)
                stream.push_back(Value::stringScalar(a.stringElem(j), p));
            continue;
        }
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

Value sprintf(const Value &fmt, Span<const Value> args, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (!fmt.isChar())
        return Value::fromString("", p);
    std::string result = formatCyclic(fmt.toString(), args, 0, mr);
    return Value::fromString(result, p);
}

// ════════════════════════════════════════════════════════════════════════
// Adapter
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void sprintf_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::fromString("", mr);
        return;
    }
    Span<const Value> rest{args.data() + 1, args.size() - 1};
    outs[0] = sprintf(args[0], rest, mr);
}

} // namespace detail

} // namespace numkit::builtin
