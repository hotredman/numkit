// toolboxes/builtin/src/datatypes/strings/format.cpp

#include <numkit/builtin/language/strings/format.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <cctype>
#include <cmath>
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

// Strip the trailing conversion char (and any l/h length modifiers) from a
// printf spec, leaving "%[flags][width][.precision]".
std::string specBody(const std::string &spec)
{
    std::string base = spec.substr(0, spec.size() - 1); // drop type char
    while (!base.empty() && (base.back() == 'l' || base.back() == 'h'))
        base.pop_back();
    return base;
}

// Format a non-finite value the way MATLAB does: Inf / -Inf / NaN (capital),
// honouring the field width and the '+'/' ' sign flags (only on +Inf; NaN
// never takes a sign). Precision is ignored. Used for both float (%f/%e/%g)
// and integer (%d/...) conversions.
std::string formatNonFinite(const std::string &spec, double v)
{
    std::string word = std::isnan(v) ? "NaN" : (v < 0 ? "-Inf" : "Inf");

    bool leftAlign = false, plus = false, space = false;
    size_t k = 1; // skip '%'
    for (; k < spec.size(); ++k) {
        const char c = spec[k];
        if (c == '-') leftAlign = true;
        else if (c == '+') plus = true;
        else if (c == ' ') space = true;
        else if (c == '0' || c == '#') { /* flag */ }
        else break;
    }
    size_t width = 0;
    for (; k < spec.size() && std::isdigit(static_cast<unsigned char>(spec[k])); ++k)
        width = width * 10 + static_cast<size_t>(spec[k] - '0');

    if (word == "Inf") {
        if (plus) word = "+Inf";
        else if (space) word = " Inf";
    }
    if (width > word.size()) {
        const std::string pad(width - word.size(), ' ');
        return leftAlign ? word + pad : pad + word;
    }
    return word;
}

// Format an integer conversion (%d/%i/%u/%o/%x/%X). MATLAB semantics:
//   - a finite whole number prints as an integer;
//   - a non-integer value falls back to %e, keeping flags/width/precision
//     (e.g. sprintf('%d',3.7) -> '3.700000e+00', sprintf('%.2d',3.7) ->
//     '3.70e+00');
//   - Inf / -Inf / NaN print as 'Inf' / '-Inf' / 'NaN' (width honoured).
std::string formatIntegerConv(const std::string &spec, char type, double v)
{
    char buf[160];
    if (!std::isfinite(v))
        return formatNonFinite(spec, v);

    const bool whole =
        (v == std::trunc(v)) && std::fabs(v) < 9.2e18; // fits in int64
    if (whole) {
        const std::string body = specBody(spec);
        if (type == 'u') {
            std::snprintf(buf, sizeof(buf), (body + "llu").c_str(),
                          static_cast<unsigned long long>(static_cast<long long>(v)));
        } else if (type == 'x' || type == 'X') {
            std::string xs = body + "ll";
            xs.push_back(type);
            std::snprintf(buf, sizeof(buf), xs.c_str(),
                          static_cast<unsigned long long>(static_cast<long long>(v)));
        } else if (type == 'o') {
            std::snprintf(buf, sizeof(buf), (body + "llo").c_str(),
                          static_cast<unsigned long long>(static_cast<long long>(v)));
        } else { // d, i
            std::snprintf(buf, sizeof(buf), (body + "lld").c_str(),
                          static_cast<long long>(v));
        }
        return buf;
    }

    // Non-integer → %e fallback (MATLAB overrides the integer conversion).
    std::snprintf(buf, sizeof(buf), (specBody(spec) + "e").c_str(), v);
    return buf;
}

} // namespace

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

std::string formatOnce(const std::string &fmt, Span<const Value> args, size_t argStart,
                       bool literalWhenShort)
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
            // The verbatim spec text (before any '*' splicing), emitted as a
            // literal when this conversion runs out of arguments and the
            // caller asked for that behaviour (MATLAB compose short chunks).
            const std::string literalSpec = spec;

            // Resolve C-style '*' field width / precision taken from the
            // argument list (MATLAB supports this: sprintf('%*.*f',8,2,pi)).
            // Each '*' consumes one numeric arg, in order (width then
            // precision), BEFORE the conversion consumes its value arg. We
            // splice the concrete integer into the spec so the downstream
            // string / integer / float formatters need no '*' awareness.
            for (size_t sp = spec.find('*'); sp != std::string::npos;
                 sp = spec.find('*')) {
                long w = (ai < args.size())
                             ? static_cast<long>(args[ai].toScalar())
                             : 0;
                if (ai < args.size()) ++ai;
                spec.replace(sp, 1, std::to_string(w));
            }

            if (type == 's') {
                // MATLAB %s accepts both char arrays and string scalars, and
                // honours width / precision in the spec (e.g. %5s, %-5s, %.1s).
                if (ai < args.size()
                    && (args[ai].isChar() || args[ai].isString()))
                    out << applyStringSpec(spec, args[ai].toString());
                else if (literalWhenShort && ai >= args.size())
                    out << literalSpec;
                ai++;
            } else if (type == 'c') {
                if (ai < args.size()) {
                    if (args[ai].isChar()) {
                        std::string s = args[ai].toString();
                        out << (s.empty() ? ' ' : s[0]);
                    } else {
                        out << static_cast<char>(static_cast<int>(args[ai].toScalar()));
                    }
                } else if (literalWhenShort) {
                    out << literalSpec;
                }
                ai++;
            } else if (type == 'd' || type == 'i' || type == 'u'
                       || type == 'x' || type == 'X' || type == 'o') {
                if (ai < args.size())
                    out << formatIntegerConv(spec, type, args[ai].toScalar());
                else if (literalWhenShort)
                    out << literalSpec;
                ai++;
            } else if (type == 'f' || type == 'e' || type == 'E' || type == 'g' || type == 'G') {
                if (ai < args.size()) {
                    const double v = args[ai].toScalar();
                    if (!std::isfinite(v)) {
                        // MATLAB prints Inf / -Inf / NaN (capitalised) rather
                        // than the C library's lowercase inf/nan.
                        out << formatNonFinite(spec, v);
                    } else {
                        char buf[128];
                        std::snprintf(buf, sizeof(buf), spec.c_str(), v);
                        out << buf;
                    }
                } else if (literalWhenShort) {
                    out << literalSpec;
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
        // flags
        while (i < fmt.size()
               && (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == '0' || fmt[i] == ' '
                   || fmt[i] == '#'))
            ++i;
        // width: '*' consumes one arg from the list, else literal digits
        if (i < fmt.size() && fmt[i] == '*') { ++n; ++i; }
        else while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) ++i;
        // precision: '.' then '*' (one arg) or literal digits
        if (i < fmt.size() && fmt[i] == '.') {
            ++i;
            if (i < fmt.size() && fmt[i] == '*') { ++n; ++i; }
            else while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) ++i;
        }
        while (i < fmt.size() && (fmt[i] == 'l' || fmt[i] == 'h'))
            ++i;
        if (i < fmt.size()) ++n;   // the conversion itself consumes the value arg
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
            // MATLAB uses the REAL part of a complex argument for every numeric
            // conversion (sprintf('%g',1+2i) -> "1"); the imaginary part is
            // discarded. Push the real part so formatOnce's toScalar succeeds.
            if (a.type() == ValueType::COMPLEX)
                stream.push_back(Value::scalar(a.complexData()[0].real(), p));
            else
                stream.push_back(a);
            continue;
        }
        size_t n = a.numel();
        for (size_t j = 0; j < n; ++j) {
            double v;
            if (a.type() == ValueType::DOUBLE) v = a.doubleData()[j];
            else if (a.isLogical())        v = a.logicalData()[j] ? 1.0 : 0.0;
            else if (a.type() == ValueType::COMPLEX) v = a.complexData()[j].real();
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

} // namespace numkit::builtin
