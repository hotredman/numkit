// libs/builtin/src/datatypes/strings/strings.cpp
//
// Non-regex string builtins: num2str / str2num / str2double / string /
// char / strcmp / strcmpi / upper / lower / strtrim / strsplit / strcat /
// strlength / strrep / contains / startsWith / endsWith. The regex
// builtins (regexp/regexpi/regexprep) live in regex.cpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/language/strings/strings.hpp>
#include <numkit/builtin/language/strings/format.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

// ── Conversion ──────────────────────────────────────────────────────────

// MATLAB num2str signatures (R2025b):
//   num2str(X)         → ~5 significant digits (default)
//   num2str(X, N)      → N significant digits, where N is integer
//   num2str(X, FMT)    → printf-style format
// See BUGS.md #26.
Value num2str(std::pmr::memory_resource *mr, const Value &x)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.5g", x.toScalar());
    return Value::fromString(std::string(buf), mr);
}

Value num2str(std::pmr::memory_resource *mr, const Value &x, const Value &spec)
{
    const double v = x.toScalar();
    char buf[256];
    if (spec.isChar() || spec.isString()) {
        // Format-string form: pass through sprintf. Single-arg only;
        // MATLAB allows multi-arg formats but our path is scalar.
        const std::string fmt = spec.toString();
        std::snprintf(buf, sizeof(buf), fmt.c_str(), v);
        return Value::fromString(std::string(buf), mr);
    }
    // Numeric N: N significant digits via %.<N>g.
    int n = static_cast<int>(spec.toScalar());
    if (n < 1)  n = 1;
    if (n > 99) n = 99;
    char fmt[16];
    std::snprintf(fmt, sizeof(fmt), "%%.%dg", n);
    std::snprintf(buf, sizeof(buf), fmt, v);
    return Value::fromString(std::string(buf), mr);
}

Value str2num(std::pmr::memory_resource *mr, const Value &s)
{
    try {
        return Value::scalar(std::stod(s.toString()), mr);
    } catch (...) {
        return Value::empty();
    }
}

Value str2double(std::pmr::memory_resource *mr, const Value &s)
{
    try {
        return Value::scalar(std::stod(s.toString()), mr);
    } catch (...) {
        return Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr);
    }
}

Value toString(std::pmr::memory_resource *mr, const Value &x)
{
    std::pmr::memory_resource *p = mr;
    if (x.isString())
        return x;
    if (x.isChar())
        return Value::stringScalar(x.toString(), p);
    if (x.isNumeric()) {
        if (x.isScalar()) {
            std::ostringstream os;
            os << x.toScalar();
            return Value::stringScalar(os.str(), p);
        }
        auto result = Value::stringArray(x.dims().rows(), x.dims().cols());
        for (size_t i = 0; i < x.numel(); ++i) {
            std::ostringstream os;
            os << x.doubleData()[i];
            result.stringElemSet(i, os.str());
        }
        return result;
    }
    if (x.isLogical())
        return Value::stringScalar(x.toBool() ? "true" : "false", p);
    throw Error("Cannot convert input to string", 0, 0, "string", "",
                 "m:string:unsupportedType");
}

Value toChar(std::pmr::memory_resource *mr, const Value &x)
{
    std::pmr::memory_resource *p = mr;
    if (x.isChar())
        return x;
    if (x.isString())
        return Value::fromString(x.toString(), p);
    if (x.isNumeric()) {
        std::string s;
        if (x.isScalar()) {
            s += static_cast<char>(static_cast<int>(x.toScalar()));
        } else {
            const double *d = x.doubleData();
            for (size_t i = 0; i < x.numel(); ++i)
                s += static_cast<char>(static_cast<int>(d[i]));
        }
        return Value::fromString(s, p);
    }
    throw Error("Cannot convert to char", 0, 0, "char", "", "m:char:unsupportedType");
}

// ── Comparisons ─────────────────────────────────────────────────────────

Value strcmp(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    return Value::logicalScalar(a.toString() == b.toString(), mr);
}

Value strcmpi(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    std::string sa = a.toString(), sb = b.toString();
    std::transform(sa.begin(), sa.end(), sa.begin(), ::tolower);
    std::transform(sb.begin(), sb.end(), sb.begin(), ::tolower);
    return Value::logicalScalar(sa == sb, mr);
}

Value strncmp(std::pmr::memory_resource *mr, const Value &a, const Value &b, size_t n)
{
    std::string sa = a.toString(), sb = b.toString();
    if (sa.size() < n || sb.size() < n) return Value::logicalScalar(false, mr);
    return Value::logicalScalar(sa.compare(0, n, sb, 0, n) == 0, mr);
}

Value strncmpi(std::pmr::memory_resource *mr, const Value &a, const Value &b, size_t n)
{
    std::string sa = a.toString(), sb = b.toString();
    if (sa.size() < n || sb.size() < n) return Value::logicalScalar(false, mr);
    std::transform(sa.begin(), sa.begin() + n, sa.begin(), ::tolower);
    std::transform(sb.begin(), sb.begin() + n, sb.begin(), ::tolower);
    return Value::logicalScalar(sa.compare(0, n, sb, 0, n) == 0, mr);
}

// ── Case transforms ─────────────────────────────────────────────────────

Value upper(std::pmr::memory_resource *mr, const Value &s)
{
    std::string r = s.toString();
    std::transform(r.begin(), r.end(), r.begin(), ::toupper);
    return Value::fromString(r, mr);
}

Value lower(std::pmr::memory_resource *mr, const Value &s)
{
    std::string r = s.toString();
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return Value::fromString(r, mr);
}

// ── Trim / split / concat ───────────────────────────────────────────────

Value strtrim(std::pmr::memory_resource *mr, const Value &s)
{
    std::string r = s.toString();
    size_t start = r.find_first_not_of(" \t\r\n");
    size_t end = r.find_last_not_of(" \t\r\n");
    if (start == std::string::npos)
        return Value::fromString("", mr);
    return Value::fromString(r.substr(start, end - start + 1), mr);
}

Value deblank(std::pmr::memory_resource *mr, const Value &s)
{
    std::string r = s.toString();
    size_t end = r.find_last_not_of(" \t\r\n\f\v");
    if (end == std::string::npos)
        return Value::fromString("", mr);
    return Value::fromString(r.substr(0, end + 1), mr);
}

Value blanks(std::pmr::memory_resource *mr, size_t n)
{
    return Value::fromString(std::string(n, ' '), mr);
}

namespace {

Value strsplitImpl(std::pmr::memory_resource *mr, const std::string &s, char delim)
{
    ScratchArena scratch(mr);
    ScratchVec<std::string> parts(&scratch);
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, delim))
        if (!token.empty())
            parts.push_back(token);
    auto c = Value::cell(1, parts.size());
    for (size_t i = 0; i < parts.size(); ++i)
        c.cellAt(i) = Value::fromString(parts[i], mr);
    return c;
}

} // namespace

Value strsplit(std::pmr::memory_resource *mr, const Value &s)
{
    return strsplitImpl(mr, s.toString(), ' ');
}

Value strsplit(std::pmr::memory_resource *mr, const Value &s, const Value &delim)
{
    std::string d = delim.toString();
    char ch = d.empty() ? ' ' : d[0];
    return strsplitImpl(mr, s.toString(), ch);
}

Value strcat(std::pmr::memory_resource *mr, Span<const Value> parts)
{
    std::string result;
    for (const auto &p : parts)
        result += p.toString();
    return Value::fromString(result, mr);
}

// ── Length ──────────────────────────────────────────────────────────────

Value strlength(std::pmr::memory_resource *mr, const Value &s)
{
    std::pmr::memory_resource *p = mr;
    if (s.isString()) {
        if (s.isScalar())
            return Value::scalar(static_cast<double>(s.toString().size()), p);
        auto result = createLike(s, ValueType::DOUBLE, p);
        for (size_t i = 0; i < s.numel(); ++i)
            result.doubleDataMut()[i] = static_cast<double>(s.stringElem(i).size());
        return result;
    }
    if (s.isChar())
        return Value::scalar(static_cast<double>(s.numel()), p);
    throw Error("Input must be a string or char array", 0, 0, "strlength", "",
                 "m:strlength:unsupportedType");
}

// ── Search / replace ────────────────────────────────────────────────────

Value strfind(std::pmr::memory_resource *mr, const Value &s, const Value &pat)
{
    const std::string ss = s.toString();
    const std::string pp = pat.toString();
    if (pp.empty()) {
        // MATLAB strfind('hello', '') returns [].
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    }
    ScratchArena scratch(mr);
    ScratchVec<size_t> hits(&scratch);
    size_t pos = 0;
    while ((pos = ss.find(pp, pos)) != std::string::npos) {
        hits.push_back(pos + 1);  // 1-based
        pos += 1;                 // overlapping matches like MATLAB
    }
    if (hits.empty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto r = Value::matrix(1, hits.size(), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < hits.size(); ++i)
        r.doubleDataMut()[i] = static_cast<double>(hits[i]);
    return r;
}

Value mat2str(std::pmr::memory_resource *mr, const Value &x, int precision)
{
    if (x.isEmpty())
        return Value::fromString("[]", mr);

    auto fmt = [precision](double v) {
        std::ostringstream os;
        os.precision(precision);
        os << v;
        return os.str();
    };

    const auto &d = x.dims();
    if (d.ndim() > 2)
        throw Error("mat2str: only 2-D inputs are supported",
                     0, 0, "mat2str", "", "m:mat2str:rank");
    const size_t R = d.rows(), C = d.cols();

    if (x.isScalar()) {
        return Value::fromString(fmt(x.toScalar()), mr);
    }

    std::string out;
    out.reserve(R * C * (precision + 4) + R + 4);
    out.push_back('[');
    for (size_t r = 0; r < R; ++r) {
        if (r > 0) out.push_back(';');
        for (size_t c = 0; c < C; ++c) {
            if (c > 0) out.push_back(' ');
            out += fmt(x.doubleData()[c * R + r]);
        }
    }
    out.push_back(']');
    return Value::fromString(out, mr);
}

Value strjoin(std::pmr::memory_resource *mr, const Value &c, const Value *delim)
{
    if (!c.isCell())
        throw Error("strjoin: first argument must be a cell array",
                     0, 0, "strjoin", "", "m:strjoin:notCell");
    const std::string sep = delim ? delim->toString() : std::string(" ");
    std::string out;
    const size_t n = c.numel();
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) out += sep;
        out += c.cellAt(i).toString();
    }
    return Value::fromString(out, mr);
}

// ── Pack 18 ──────────────────────────────────────────────────────────

Value append(std::pmr::memory_resource *mr, Span<const Value> parts)
{
    // Same shape as strcat but does NOT trim trailing whitespace from
    // char-array operands (MATLAB's "append" is the literal-concatenate
    // variant that script users reach for).
    std::string out;
    for (const auto &p : parts)
        out += p.toString();
    return Value::fromString(out, mr);
}

Value count(std::pmr::memory_resource *mr, const Value &s, const Value &pat)
{
    const std::string ss = s.toString();
    const std::string pp = pat.toString();
    if (pp.empty()) return Value::scalar(0.0, mr);
    size_t n = 0, pos = 0;
    while ((pos = ss.find(pp, pos)) != std::string::npos) {
        ++n;
        pos += pp.size();   // non-overlapping (matches MATLAB)
    }
    return Value::scalar(static_cast<double>(n), mr);
}

Value erase(std::pmr::memory_resource *mr, const Value &s, const Value &pat)
{
    std::string r = s.toString();
    const std::string pp = pat.toString();
    if (pp.empty()) {
        if (s.isString()) return Value::stringScalar(r, mr);
        return Value::fromString(r, mr);
    }
    size_t pos = 0;
    while ((pos = r.find(pp, pos)) != std::string::npos)
        r.erase(pos, pp.size());
    if (s.isString()) return Value::stringScalar(r, mr);
    return Value::fromString(r, mr);
}

Value replace(std::pmr::memory_resource *mr, const Value &s,
              const Value &oldPat, const Value &newPat)
{
    return strrep(mr, s, oldPat, newPat);
}

Value reverse(std::pmr::memory_resource *mr, const Value &s)
{
    std::string r = s.toString();
    std::reverse(r.begin(), r.end());
    if (s.isString()) return Value::stringScalar(r, mr);
    return Value::fromString(r, mr);
}

Value splitlines(std::pmr::memory_resource *mr, const Value &s)
{
    const std::string ss = s.toString();
    ScratchArena scratch(mr);
    ScratchVec<std::string> parts(&scratch);
    std::string cur;
    for (size_t i = 0; i < ss.size(); ++i) {
        const char c = ss[i];
        if (c == '\r') {
            parts.push_back(std::move(cur));
            cur.clear();
            // Skip a following \n to handle CRLF as one separator.
            if (i + 1 < ss.size() && ss[i + 1] == '\n') ++i;
        } else if (c == '\n') {
            parts.push_back(std::move(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    // Always push the final segment, even if empty (matches MATLAB:
    // splitlines("a\nb") returns 2 entries; splitlines("a\n") returns 2).
    parts.push_back(std::move(cur));
    auto out = Value::cell(parts.size(), 1, mr);
    for (size_t i = 0; i < parts.size(); ++i)
        out.cellAt(i) = Value::fromString(parts[i], mr);
    return out;
}

namespace {
inline std::string readSide(const Value *side, const char *def)
{
    if (!side) return def;
    if (!side->isChar() && !side->isString())
        throw std::runtime_error("string-side argument must be a string");
    auto s = side->toString();
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
} // anon

Value pad(std::pmr::memory_resource *mr, const Value &s, size_t n,
          const Value *side, const Value *padChar)
{
    std::string r = s.toString();
    if (r.size() >= n) {
        if (s.isString()) return Value::stringScalar(r, mr);
        return Value::fromString(r, mr);
    }
    const std::string sd = readSide(side, "right");
    char ch = ' ';
    if (padChar && (padChar->isChar() || padChar->isString())) {
        const auto p = padChar->toString();
        if (!p.empty()) ch = p[0];
    }
    const size_t pad = n - r.size();
    if (sd == "right") {
        r.append(pad, ch);
    } else if (sd == "left") {
        r.insert(r.begin(), pad, ch);
    } else if (sd == "both") {
        const size_t left = pad / 2;
        const size_t right = pad - left;
        r.insert(r.begin(), left, ch);
        r.append(right, ch);
    } else {
        throw Error("pad: side must be 'left', 'right', or 'both'",
                     0, 0, "pad", "", "m:pad:badSide");
    }
    if (s.isString()) return Value::stringScalar(r, mr);
    return Value::fromString(r, mr);
}

Value strip(std::pmr::memory_resource *mr, const Value &s,
            const Value *side, const Value *ch)
{
    std::string r = s.toString();
    const std::string sd = readSide(side, "both");
    std::string charsToStrip = " \t\r\n\f\v";
    if (ch && (ch->isChar() || ch->isString())) {
        charsToStrip = ch->toString();
        if (charsToStrip.empty()) {
            if (s.isString()) return Value::stringScalar(r, mr);
            return Value::fromString(r, mr);
        }
    }
    auto stripLeft = [&]() {
        size_t i = 0;
        while (i < r.size() && charsToStrip.find(r[i]) != std::string::npos) ++i;
        if (i > 0) r.erase(0, i);
    };
    auto stripRight = [&]() {
        while (!r.empty() && charsToStrip.find(r.back()) != std::string::npos)
            r.pop_back();
    };
    if (sd == "left" || sd == "both") stripLeft();
    if (sd == "right" || sd == "both") stripRight();
    if (s.isString()) return Value::stringScalar(r, mr);
    return Value::fromString(r, mr);
}

Value matches(std::pmr::memory_resource *mr, const Value &s, const Value &pat)
{
    const std::string ss = s.toString();
    if (pat.isCell()) {
        // True iff s equals any element of pat.
        for (size_t i = 0; i < pat.numel(); ++i) {
            if (ss == pat.cellAt(i).toString())
                return Value::logicalScalar(true, mr);
        }
        return Value::logicalScalar(false, mr);
    }
    return Value::logicalScalar(ss == pat.toString(), mr);
}

// ── Pack 21 ──────────────────────────────────────────────────────────

Value convertCharsToStrings(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isString()) return x;
    if (x.isChar())   return Value::stringScalar(x.toString(), mr);
    if (x.isCell()) {
        const auto &d = x.dims();
        auto c = d.is3D()
                    ? Value::cell3D(d.rows(), d.cols(), d.pages(), mr)
                    : Value::cell(d.rows(), d.cols(), mr);
        for (size_t i = 0; i < x.numel(); ++i) {
            const auto &e = x.cellAt(i);
            if (e.isChar())
                c.cellAt(i) = Value::stringScalar(e.toString(), mr);
            else
                c.cellAt(i) = e;
        }
        return c;
    }
    // Numeric / other types: pass through unchanged.
    return x;
}

Value convertStringsToChars(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isChar()) return x;
    if (x.isString()) {
        if (x.numel() <= 1) {
            return Value::fromString(x.toString(), mr);
        }
        // Multi-element string → cell of char rows.
        const auto &d = x.dims();
        auto c = d.is3D()
                    ? Value::cell3D(d.rows(), d.cols(), d.pages(), mr)
                    : Value::cell(d.rows(), d.cols(), mr);
        for (size_t i = 0; i < x.numel(); ++i)
            c.cellAt(i) = Value::fromString(x.stringElem(i), mr);
        return c;
    }
    if (x.isCell()) {
        const auto &d = x.dims();
        auto c = d.is3D()
                    ? Value::cell3D(d.rows(), d.cols(), d.pages(), mr)
                    : Value::cell(d.rows(), d.cols(), mr);
        for (size_t i = 0; i < x.numel(); ++i) {
            const auto &e = x.cellAt(i);
            if (e.isString())
                c.cellAt(i) = Value::fromString(e.toString(), mr);
            else
                c.cellAt(i) = e;
        }
        return c;
    }
    return x;
}

Value isstringscalar(std::pmr::memory_resource *mr, const Value &x)
{
    return Value::logicalScalar(x.isString() && x.numel() == 1, mr);
}

namespace {
// Build a logical array shaped like the input char/string by running
// `predFn(c)` over each character.
template <typename PredFn>
Value applyCharPred(std::pmr::memory_resource *mr, const Value &s, PredFn pred)
{
    if (s.isChar()) {
        const std::string str = s.toString();
        const size_t n = str.size();
        // Mirror char's shape (1×N for typical char rows).
        const auto &d = s.dims();
        auto r = (d.is3D()
                     ? Value::matrix3d(d.rows(), d.cols(), d.pages(),
                                       ValueType::LOGICAL, mr)
                     : Value::matrix(d.rows(), d.cols(),
                                     ValueType::LOGICAL, mr));
        for (size_t i = 0; i < n && i < r.numel(); ++i)
            r.logicalDataMut()[i] = pred(static_cast<unsigned char>(str[i])) ? 1 : 0;
        return r;
    }
    if (s.isString()) {
        // For string array, MATLAB returns a cell array of logical
        // arrays (one per element). For now support scalar strings.
        if (s.numel() == 1) {
            const std::string str = s.toString();
            auto r = Value::matrix(1, str.size(), ValueType::LOGICAL, mr);
            for (size_t i = 0; i < str.size(); ++i)
                r.logicalDataMut()[i] = pred(static_cast<unsigned char>(str[i])) ? 1 : 0;
            return r;
        }
        throw Error("char-predicate: string-array form not supported",
                     0, 0, "isstrprop", "", "m:isstrprop:stringArray");
    }
    throw Error("char-predicate: input must be char or string",
                 0, 0, "isstrprop", "", "m:isstrprop:type");
}
} // anon

Value isstrprop(std::pmr::memory_resource *mr, const Value &s, const Value &category)
{
    if (!category.isChar() && !category.isString())
        throw Error("isstrprop: category must be a string",
                     0, 0, "isstrprop", "", "m:isstrprop:cat");
    auto cat = category.toString();
    for (auto &c : cat) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (cat == "alpha")
        return applyCharPred(mr, s, [](unsigned char c) { return std::isalpha(c) != 0; });
    if (cat == "digit")
        return applyCharPred(mr, s, [](unsigned char c) { return std::isdigit(c) != 0; });
    if (cat == "alphanum")
        return applyCharPred(mr, s, [](unsigned char c) { return std::isalnum(c) != 0; });
    if (cat == "lower")
        return applyCharPred(mr, s, [](unsigned char c) { return std::islower(c) != 0; });
    if (cat == "upper")
        return applyCharPred(mr, s, [](unsigned char c) { return std::isupper(c) != 0; });
    if (cat == "punct")
        return applyCharPred(mr, s, [](unsigned char c) { return std::ispunct(c) != 0; });
    if (cat == "space" || cat == "wspace")
        return applyCharPred(mr, s, [](unsigned char c) { return std::isspace(c) != 0; });
    if (cat == "xdigit")
        return applyCharPred(mr, s, [](unsigned char c) { return std::isxdigit(c) != 0; });
    if (cat == "cntrl")
        return applyCharPred(mr, s, [](unsigned char c) { return std::iscntrl(c) != 0; });
    if (cat == "graphic")
        return applyCharPred(mr, s, [](unsigned char c) { return std::isgraph(c) != 0; });
    if (cat == "print")
        return applyCharPred(mr, s, [](unsigned char c) { return std::isprint(c) != 0; });
    throw Error("isstrprop: unknown category '" + cat + "'",
                 0, 0, "isstrprop", "", "m:isstrprop:badCat");
}

Value isletter(std::pmr::memory_resource *mr, const Value &s)
{
    return applyCharPred(mr, s, [](unsigned char c) { return std::isalpha(c) != 0; });
}

Value isspaceFn(std::pmr::memory_resource *mr, const Value &s)
{
    return applyCharPred(mr, s, [](unsigned char c) { return std::isspace(c) != 0; });
}

// ── Pack 22 ──────────────────────────────────────────────────────────
//
// Position-or-pattern lookup: numeric scalar → 0-based index, string
// → find first occurrence and return its [begin, end) range.

namespace {
struct PosRange { size_t begin; size_t end; bool found; };

// Resolve `p` to a [begin, end) range in `s`. For a numeric position
// p, [p-1, p) is returned (so callers can treat it as a "single
// character" anchor). For a string pattern, [pos, pos+len).
PosRange resolvePos(const std::string &s, const Value &p)
{
    if (p.isChar() || p.isString()) {
        const std::string pat = p.toString();
        if (pat.empty()) return {0, 0, false};
        const size_t pos = s.find(pat);
        if (pos == std::string::npos) return {0, 0, false};
        return {pos, pos + pat.size(), true};
    }
    // Numeric scalar (1-based).
    const auto p1 = static_cast<long long>(p.toScalar());
    if (p1 < 1 || static_cast<size_t>(p1) > s.size())
        return {0, 0, false};
    return {static_cast<size_t>(p1 - 1), static_cast<size_t>(p1), true};
}

inline Value strLikeOf(std::pmr::memory_resource *mr, const Value &s,
                       const std::string &out)
{
    if (s.isString()) return Value::stringScalar(out, mr);
    return Value::fromString(out, mr);
}
} // anon

Value extractAfter(std::pmr::memory_resource *mr, const Value &s, const Value &p)
{
    const std::string ss = s.toString();
    const auto r = resolvePos(ss, p);
    if (!r.found) return strLikeOf(mr, s, "");
    return strLikeOf(mr, s, ss.substr(r.end));
}

Value extractBefore(std::pmr::memory_resource *mr, const Value &s, const Value &p)
{
    const std::string ss = s.toString();
    const auto r = resolvePos(ss, p);
    if (!r.found) return strLikeOf(mr, s, "");
    return strLikeOf(mr, s, ss.substr(0, r.begin));
}

Value extractBetween(std::pmr::memory_resource *mr, const Value &s,
                     const Value &start, const Value &end)
{
    const std::string ss = s.toString();
    const auto rs = resolvePos(ss, start);
    if (!rs.found) return strLikeOf(mr, s, "");
    // For "between", search for the end pattern only after the start
    // pattern's tail to avoid hitting the same span.
    const std::string tail = ss.substr(rs.end);
    const auto re = resolvePos(tail, end);
    if (!re.found) return strLikeOf(mr, s, "");
    return strLikeOf(mr, s, tail.substr(0, re.begin));
}

Value insertAfter(std::pmr::memory_resource *mr, const Value &s,
                  const Value &p, const Value &newText)
{
    std::string ss = s.toString();
    const auto r = resolvePos(ss, p);
    if (!r.found) return s;
    ss.insert(r.end, newText.toString());
    return strLikeOf(mr, s, ss);
}

Value insertBefore(std::pmr::memory_resource *mr, const Value &s,
                   const Value &p, const Value &newText)
{
    std::string ss = s.toString();
    const auto r = resolvePos(ss, p);
    if (!r.found) return s;
    ss.insert(r.begin, newText.toString());
    return strLikeOf(mr, s, ss);
}

Value eraseBetween(std::pmr::memory_resource *mr, const Value &s,
                   const Value &start, const Value &end)
{
    std::string ss = s.toString();
    const auto rs = resolvePos(ss, start);
    if (!rs.found) return s;
    const std::string tail = ss.substr(rs.end);
    const auto re = resolvePos(tail, end);
    if (!re.found) return s;
    // Remove the substring (rs.end, rs.end + re.begin).
    ss.erase(rs.end, re.begin);
    return strLikeOf(mr, s, ss);
}

Value replaceBetween(std::pmr::memory_resource *mr, const Value &s,
                     const Value &start, const Value &end,
                     const Value &newText)
{
    std::string ss = s.toString();
    const auto rs = resolvePos(ss, start);
    if (!rs.found) return s;
    const std::string tail = ss.substr(rs.end);
    const auto re = resolvePos(tail, end);
    if (!re.found) return s;
    ss.replace(rs.end, re.begin, newText.toString());
    return strLikeOf(mr, s, ss);
}

// ── Pack 23 ──────────────────────────────────────────────────────────

namespace {
std::string toBaseString(uint64_t v, int base, int minWidth)
{
    if (v == 0) {
        std::string s(std::max(1, minWidth), '0');
        return s;
    }
    std::string out;
    while (v > 0) {
        const int d = static_cast<int>(v % static_cast<uint64_t>(base));
        out.push_back(d < 10 ? char('0' + d) : char('A' + d - 10));
        v /= static_cast<uint64_t>(base);
    }
    while (static_cast<int>(out.size()) < minWidth) out.push_back('0');
    std::reverse(out.begin(), out.end());
    return out;
}

uint64_t parseBase(const std::string &s, int base)
{
    uint64_t v = 0;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F') d = 10 + (c - 'A');
        else throw Error(std::string("invalid digit '") + c + "' for base "
                          + std::to_string(base),
                          0, 0, "base", "", "m:base:badDigit");
        if (d >= base)
            throw Error(std::string("digit '") + c + "' out of range for base "
                          + std::to_string(base),
                          0, 0, "base", "", "m:base:badDigit");
        v = v * static_cast<uint64_t>(base) + static_cast<uint64_t>(d);
    }
    return v;
}

// Convert an N-element double vector to a 2-D char matrix where each
// row holds the base-`base` representation, padded to the maximum
// observed width (and at least minWidth).
Value vecToBaseMatrix(std::pmr::memory_resource *mr, const Value &d,
                      int base, int minWidth)
{
    const size_t n = d.numel();
    if (n == 0) return Value::fromString("", mr);
    if (n == 1) {
        const double v = d.toScalar();
        if (v < 0) throw Error("dec2*: value must be non-negative",
                                0, 0, "dec2", "", "m:dec2:negative");
        return Value::fromString(
            toBaseString(static_cast<uint64_t>(v), base, minWidth), mr);
    }
    // Compute max width.
    ScratchArena scratch(mr);
    auto rows = ScratchVec<std::string>(&scratch);
    rows.reserve(n);
    int maxW = minWidth;
    for (size_t i = 0; i < n; ++i) {
        const double v = d.elemAsDouble(i);
        if (v < 0) throw Error("dec2*: value must be non-negative",
                                0, 0, "dec2", "", "m:dec2:negative");
        rows.emplace_back(toBaseString(static_cast<uint64_t>(v), base, 0));
        maxW = std::max<int>(maxW, static_cast<int>(rows.back().size()));
    }
    // Pad each row to maxW with leading zeros.
    for (auto &r : rows)
        while (static_cast<int>(r.size()) < maxW)
            r.insert(r.begin(), '0');
    // Build a CHAR matrix n × maxW, column-major.
    auto m = Value::matrix(n, maxW, ValueType::CHAR, mr);
    char *dst = static_cast<char *>(m.rawDataMut());
    for (size_t r = 0; r < n; ++r)
        for (int c = 0; c < maxW; ++c)
            dst[c * n + r] = rows[r][c];
    return m;
}
} // anon

Value dec2bin(std::pmr::memory_resource *mr, const Value &d, int minWidth)
{
    return vecToBaseMatrix(mr, d, 2, minWidth);
}

Value dec2hex(std::pmr::memory_resource *mr, const Value &d, int minWidth)
{
    return vecToBaseMatrix(mr, d, 16, minWidth);
}

Value bin2dec(std::pmr::memory_resource *mr, const Value &s)
{
    return Value::scalar(static_cast<double>(parseBase(s.toString(), 2)), mr);
}

Value hex2dec(std::pmr::memory_resource *mr, const Value &s)
{
    return Value::scalar(static_cast<double>(parseBase(s.toString(), 16)), mr);
}

namespace {
// Stern-Brocot-style continued-fraction expansion: returns numerator
// and denominator with |x - p/q| ≤ tol·|x|, prefering small q.
std::pair<long long, long long> ratPQ(double x, double tol)
{
    if (!std::isfinite(x))
        return {0, 0};
    const double tgt = std::abs(x);
    long long sign = (x < 0) ? -1 : 1;
    double r = tgt;
    long long h0 = 1, h1 = 0;
    long long k0 = 0, k1 = 1;
    for (int i = 0; i < 64; ++i) {
        const long long a = static_cast<long long>(std::floor(r));
        const long long h = a * h0 + h1;
        const long long k = a * k0 + k1;
        h1 = h0; h0 = h;
        k1 = k0; k0 = k;
        const double approx = static_cast<double>(h0) / static_cast<double>(k0);
        if (std::abs(approx - tgt) <= tol * std::max(1.0, tgt)) break;
        const double frac = r - static_cast<double>(a);
        if (frac == 0.0) break;
        r = 1.0 / frac;
        if (r > 1e15) break;  // numerical safety
    }
    return {sign * h0, k0};
}
} // anon

Value rat(std::pmr::memory_resource *mr, const Value &x, double tol)
{
    const double v = x.toScalar();
    if (!std::isfinite(v))
        return Value::fromString(std::isnan(v) ? "NaN" : (v > 0 ? "Inf" : "-Inf"), mr);
    const auto [p, q] = ratPQ(v, tol);
    std::ostringstream os;
    if (q == 1)
        os << p;
    else
        os << p << " / " << q;
    return Value::fromString(os.str(), mr);
}

Value rats(std::pmr::memory_resource *mr, const Value &x, int len)
{
    Value r = rat(mr, x, 1e-6);
    if (len <= 0) return r;
    std::string s = r.toString();
    while (static_cast<int>(s.size()) < len) s = " " + s;
    return Value::fromString(s, mr);
}

Value strrep(std::pmr::memory_resource *mr, const Value &s, const Value &oldPat, const Value &newPat)
{
    std::pmr::memory_resource *p = mr;
    std::string r = s.toString();
    std::string op = oldPat.toString();
    std::string np = newPat.toString();
    if (!op.empty()) {
        size_t pos = 0;
        while ((pos = r.find(op, pos)) != std::string::npos) {
            r.replace(pos, op.length(), np);
            pos += np.length();
        }
    }
    if (s.isString())
        return Value::stringScalar(r, p);
    return Value::fromString(r, p);
}

Value contains(std::pmr::memory_resource *mr, const Value &s, const Value &pat)
{
    std::string ss = s.toString();
    std::string pp = pat.toString();
    return Value::logicalScalar(ss.find(pp) != std::string::npos, mr);
}

Value startsWith(std::pmr::memory_resource *mr, const Value &s, const Value &prefix)
{
    std::string ss = s.toString();
    std::string pp = prefix.toString();
    return Value::logicalScalar(
        ss.size() >= pp.size() && ss.compare(0, pp.size(), pp) == 0, mr);
}

Value endsWith(std::pmr::memory_resource *mr, const Value &s, const Value &suffix)
{
    std::string ss = s.toString();
    std::string pp = suffix.toString();
    return Value::logicalScalar(
        ss.size() >= pp.size()
            && ss.compare(ss.size() - pp.size(), pp.size(), pp) == 0,
        mr);
}

// ── Pack 36: compose / strjust / extract / split / join ──────────────

namespace {

// Read element i of x as a Value scalar to feed sprintf.
Value elemScalar(std::pmr::memory_resource *mr, const Value &x, size_t i)
{
    if (x.isNumeric()) {
        return Value::scalar(x.elemAsDouble(i), mr);
    }
    if (x.isLogical()) {
        return Value::logicalScalar(x.elemAsDouble(i) != 0, mr);
    }
    if (x.isString())  return Value::stringScalar(x.stringElem(i), mr);
    if (x.isCell())    return x.cellAt(i);  // cell-of-strings allowed
    return Value::scalar(0.0, mr);
}

// Get a char input string from char or string-scalar.
const std::string strInput(const Value &v)
{
    if (v.isChar() || (v.isString() && v.isScalar())) return v.toString();
    if (v.isString()) return v.stringElem(0);  // first elem of array
    return v.toString();  // last-resort coercion
}

// Find every non-overlapping occurrence of `needle` in `hay` and append
// each matched substring to `out`.
void findAllLiteral(const std::string &hay, const std::string &needle,
                    std::vector<std::string> &out)
{
    if (needle.empty()) return;
    size_t pos = 0;
    while (true) {
        size_t f = hay.find(needle, pos);
        if (f == std::string::npos) break;
        out.push_back(needle);
        pos = f + needle.size();
    }
}

// Split `s` by `delim` keeping empty tokens (MATLAB `split` semantics).
void splitKeepEmpty(const std::string &s, const std::string &delim,
                    std::vector<std::string> &out)
{
    if (delim.empty()) { out.push_back(s); return; }
    size_t pos = 0;
    while (true) {
        size_t f = s.find(delim, pos);
        if (f == std::string::npos) {
            out.push_back(s.substr(pos));
            return;
        }
        out.push_back(s.substr(pos, f - pos));
        pos = f + delim.size();
    }
}

} // namespace

Value compose(std::pmr::memory_resource *mr,
              const Value &fmt, const Value &x)
{
    if (!fmt.isChar() && !fmt.isString())
        throw Error("compose: format must be a char or string",
                     0, 0, "compose", "", "m:compose:badFmt");
    const std::string fmtStr = fmt.toString();

    if (x.isScalar()) {
        Value one = elemScalar(mr, x, 0);
        Value c = Value::cell(1, 1, mr);
        c.cellAt(0) = Value::fromString(formatOnce(fmtStr, {&one, 1}, 0), mr);
        return c;
    }

    const auto &dims = x.dims();
    Value c = Value::cell(dims.rows(), dims.cols(), mr);
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i) {
        Value one = elemScalar(mr, x, i);
        c.cellAt(i) = Value::fromString(formatOnce(fmtStr, {&one, 1}, 0), mr);
    }
    return c;
}

Value strjust(std::pmr::memory_resource *mr,
              const Value &M, const std::string &side)
{
    if (!M.isChar())
        throw Error("strjust: input must be a char matrix",
                     0, 0, "strjust", "", "m:strjust:badInput");
    const auto &dims = M.dims();
    const size_t rows = dims.rows();
    const size_t cols = dims.cols();
    const std::string src = M.toString();
    if (rows == 0 || cols == 0) return M;

    std::string out(rows * cols, ' ');
    for (size_t r = 0; r < rows; ++r) {
        // Column-major: row r, col c -> src[c*rows + r].
        size_t firstNonSp = cols, lastNonSp = 0;
        bool hasNonSp = false;
        for (size_t c = 0; c < cols; ++c) {
            char ch = src[c * rows + r];
            if (ch != ' ') {
                if (!hasNonSp) { firstNonSp = c; hasNonSp = true; }
                lastNonSp = c;
            }
        }
        if (!hasNonSp) continue;  // whole row blank

        const size_t len = lastNonSp - firstNonSp + 1;
        size_t target;
        if      (side == "left")   target = 0;
        else if (side == "center") target = (cols - len) / 2;
        else if (side == "right")  target = cols - len;
        else throw Error("strjust: side must be 'left', 'right', or 'center'",
                          0, 0, "strjust", "", "m:strjust:badSide");

        for (size_t c = 0; c < len; ++c)
            out[(target + c) * rows + r] = src[(firstNonSp + c) * rows + r];
    }

    // Build a char matrix of the same shape as M.
    Value r = Value::matrix(rows, cols, ValueType::CHAR, mr);
    char *p = r.charDataMut();
    std::memcpy(p, out.data(), rows * cols);
    return r;
}

Value extract(std::pmr::memory_resource *mr,
              const Value &s, const Value &pat)
{
    const std::string sStr = strInput(s);
    const std::string pStr = strInput(pat);

    std::vector<std::string> hits;
    findAllLiteral(sStr, pStr, hits);

    if (hits.empty()) return Value::cell(0, 0, mr);
    Value c = Value::cell(hits.size(), 1, mr);
    for (size_t i = 0; i < hits.size(); ++i)
        c.cellAt(i) = Value::fromString(hits[i], mr);
    return c;
}

Value split(std::pmr::memory_resource *mr,
            const Value &s, const Value &delim)
{
    const std::string sStr = strInput(s);
    const std::string dStr = strInput(delim);

    std::vector<std::string> parts;
    splitKeepEmpty(sStr, dStr, parts);

    Value c = Value::cell(parts.size(), 1, mr);
    for (size_t i = 0; i < parts.size(); ++i)
        c.cellAt(i) = Value::fromString(parts[i], mr);
    return c;
}

Value join(std::pmr::memory_resource *mr,
           const Value &arr, const Value *delim)
{
    const std::string d = delim ? strInput(*delim) : std::string(" ");

    auto getElem = [&](size_t i) -> std::string {
        if (arr.isString()) return arr.stringElem(i);
        if (arr.isCell())   return arr.cellAt(i).toString();
        return arr.toString();  // scalar char/string fallback
    };

    if (arr.isScalar()) {
        return Value::stringScalar(getElem(0), mr);
    }

    const auto &dims = arr.dims();
    const size_t rows = dims.rows();
    const size_t cols = dims.cols();

    if (rows == 1 || cols == 1) {
        // 1-D: glue all elements with `d`.
        std::string out;
        const size_t n = arr.numel();
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) out += d;
            out += getElem(i);
        }
        return Value::stringScalar(out, mr);
    }

    // 2-D: join along columns -> N×1 string column.
    Value r = Value::stringArray(rows, 1, mr);
    for (size_t row = 0; row < rows; ++row) {
        std::string out;
        for (size_t col = 0; col < cols; ++col) {
            if (col > 0) out += d;
            out += getElem(col * rows + row);
        }
        r.stringElemSet(row, out);
    }
    return r;
}

// ── Pack 36: array constructors / character constants ───────────────
Value newlineFn(std::pmr::memory_resource *mr)
{
    return Value::fromString("\n", mr);
}

Value stringsND(std::pmr::memory_resource *mr,
                const size_t *dims, size_t ndim)
{
    if (ndim == 0)
        return Value::stringScalar("", mr);

    const size_t r = dims[0];
    const size_t c = (ndim >= 2) ? dims[1] : r;  // strings(n) → n×n.
    size_t p = 0;
    if (ndim >= 3) {
        p = 1;
        for (size_t i = 2; i < ndim; ++i)
            p *= dims[i];  // collapse extras into pages (3D cap).
    }

    auto v = (p == 0) ? Value::stringArray(r, c, mr)
                      : Value::stringArray3D(r, c, p, mr);
    const size_t n = v.numel();
    for (size_t i = 0; i < n; ++i)
        v.stringElemSet(i, "");
    return v;
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void num2str_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("num2str: requires 1 argument", 0, 0, "num2str", "", "m:num2str:nargin");
    if (args.size() >= 2)
        outs[0] = num2str(ctx.engine->resource(), args[0], args[1]);
    else
        outs[0] = num2str(ctx.engine->resource(), args[0]);
}

void str2num_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("str2num: requires 1 argument", 0, 0, "str2num", "", "m:str2num:nargin");
    outs[0] = str2num(ctx.engine->resource(), args[0]);
}

void str2double_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("str2double: requires 1 argument", 0, 0, "str2double", "",
                     "m:str2double:nargin");
    outs[0] = str2double(ctx.engine->resource(), args[0]);
}

void string_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::stringScalar("", mr);
        return;
    }
    outs[0] = toString(mr, args[0]);
}

void char_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("char requires an argument", 0, 0, "char", "", "m:char:nargin");
    outs[0] = toChar(ctx.engine->resource(), args[0]);
}

void strcmp_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("strcmp: requires 2 arguments", 0, 0, "strcmp", "", "m:strcmp:nargin");
    outs[0] = strcmp(ctx.engine->resource(), args[0], args[1]);
}

void strcmpi_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("strcmpi: requires 2 arguments", 0, 0, "strcmpi", "",
                     "m:strcmpi:nargin");
    outs[0] = strcmpi(ctx.engine->resource(), args[0], args[1]);
}

void upper_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("upper: requires 1 argument", 0, 0, "upper", "", "m:upper:nargin");
    outs[0] = upper(ctx.engine->resource(), args[0]);
}

void lower_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("lower: requires 1 argument", 0, 0, "lower", "", "m:lower:nargin");
    outs[0] = lower(ctx.engine->resource(), args[0]);
}

void strtrim_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strtrim: requires 1 argument", 0, 0, "strtrim", "",
                     "m:strtrim:nargin");
    outs[0] = strtrim(ctx.engine->resource(), args[0]);
}

void strsplit_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strsplit: requires 1 argument", 0, 0, "strsplit", "",
                     "m:strsplit:nargin");
    if (args.size() == 1)
        outs[0] = strsplit(ctx.engine->resource(), args[0]);
    else
        outs[0] = strsplit(ctx.engine->resource(), args[0], args[1]);
}

void strcat_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    outs[0] = strcat(ctx.engine->resource(), args);
}

void strlength_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strlength: requires 1 argument", 0, 0, "strlength", "",
                     "m:strlength:nargin");
    outs[0] = strlength(ctx.engine->resource(), args[0]);
}

void strrep_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("strrep requires 3 arguments", 0, 0, "strrep", "", "m:strrep:nargin");
    outs[0] = strrep(ctx.engine->resource(), args[0], args[1], args[2]);
}

void contains_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("contains requires 2 arguments", 0, 0, "contains", "",
                     "m:contains:nargin");
    outs[0] = contains(ctx.engine->resource(), args[0], args[1]);
}

void startsWith_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("startsWith requires 2 arguments", 0, 0, "startsWith", "",
                     "m:startsWith:nargin");
    outs[0] = startsWith(ctx.engine->resource(), args[0], args[1]);
}

void endsWith_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("endsWith requires 2 arguments", 0, 0, "endsWith", "",
                     "m:endsWith:nargin");
    outs[0] = endsWith(ctx.engine->resource(), args[0], args[1]);
}

void strncmp_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("strncmp: requires 3 arguments", 0, 0, "strncmp", "",
                     "m:strncmp:nargin");
    const size_t n = static_cast<size_t>(args[2].toScalar());
    outs[0] = strncmp(ctx.engine->resource(), args[0], args[1], n);
}

void strncmpi_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("strncmpi: requires 3 arguments", 0, 0, "strncmpi", "",
                     "m:strncmpi:nargin");
    const size_t n = static_cast<size_t>(args[2].toScalar());
    outs[0] = strncmpi(ctx.engine->resource(), args[0], args[1], n);
}

void strfind_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("strfind: requires 2 arguments", 0, 0, "strfind", "",
                     "m:strfind:nargin");
    outs[0] = strfind(ctx.engine->resource(), args[0], args[1]);
}

void blanks_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("blanks: requires 1 argument", 0, 0, "blanks", "",
                     "m:blanks:nargin");
    const size_t n = static_cast<size_t>(args[0].toScalar());
    outs[0] = blanks(ctx.engine->resource(), n);
}

void deblank_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("deblank: requires 1 argument", 0, 0, "deblank", "",
                     "m:deblank:nargin");
    outs[0] = deblank(ctx.engine->resource(), args[0]);
}

void mat2str_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mat2str: requires at least 1 argument", 0, 0, "mat2str", "",
                     "m:mat2str:nargin");
    int prec = 15;
    if (args.size() >= 2 && !args[1].isEmpty())
        prec = static_cast<int>(args[1].toScalar());
    outs[0] = mat2str(ctx.engine->resource(), args[0], prec);
}

void strjoin_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strjoin: requires at least 1 argument", 0, 0, "strjoin", "",
                     "m:strjoin:nargin");
    const Value *delim = (args.size() >= 2) ? &args[1] : nullptr;
    outs[0] = strjoin(ctx.engine->resource(), args[0], delim);
}

void append_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    outs[0] = append(ctx.engine->resource(), args);
}

void count_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("count: requires (s, pat)",
                     0, 0, "count", "", "m:count:nargin");
    outs[0] = count(ctx.engine->resource(), args[0], args[1]);
}

void erase_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("erase: requires (s, pat)",
                     0, 0, "erase", "", "m:erase:nargin");
    outs[0] = erase(ctx.engine->resource(), args[0], args[1]);
}

void replace_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("replace: requires (s, old, new)",
                     0, 0, "replace", "", "m:replace:nargin");
    outs[0] = replace(ctx.engine->resource(), args[0], args[1], args[2]);
}

void reverse_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("reverse: requires 1 argument",
                     0, 0, "reverse", "", "m:reverse:nargin");
    outs[0] = reverse(ctx.engine->resource(), args[0]);
}

void splitlines_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("splitlines: requires 1 argument",
                     0, 0, "splitlines", "", "m:splitlines:nargin");
    outs[0] = splitlines(ctx.engine->resource(), args[0]);
}

void pad_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pad: requires (s, n[, side[, ch]])",
                     0, 0, "pad", "", "m:pad:nargin");
    const size_t n = static_cast<size_t>(args[1].toScalar());
    const Value *side = (args.size() >= 3 && !args[2].isEmpty()) ? &args[2] : nullptr;
    const Value *ch   = (args.size() >= 4 && !args[3].isEmpty()) ? &args[3] : nullptr;
    outs[0] = pad(ctx.engine->resource(), args[0], n, side, ch);
}

void strip_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strip: requires (s[, side[, ch]])",
                     0, 0, "strip", "", "m:strip:nargin");
    const Value *side = (args.size() >= 2 && !args[1].isEmpty()) ? &args[1] : nullptr;
    const Value *ch   = (args.size() >= 3 && !args[2].isEmpty()) ? &args[2] : nullptr;
    outs[0] = strip(ctx.engine->resource(), args[0], side, ch);
}

void matches_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("matches: requires (s, pat)",
                     0, 0, "matches", "", "m:matches:nargin");
    outs[0] = matches(ctx.engine->resource(), args[0], args[1]);
}

void convertCharsToStrings_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("convertCharsToStrings: requires 1 argument",
                     0, 0, "convertCharsToStrings", "", "m:convertCharsToStrings:nargin");
    outs[0] = convertCharsToStrings(ctx.engine->resource(), args[0]);
}

void convertStringsToChars_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("convertStringsToChars: requires 1 argument",
                     0, 0, "convertStringsToChars", "", "m:convertStringsToChars:nargin");
    outs[0] = convertStringsToChars(ctx.engine->resource(), args[0]);
}

void isstringscalar_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isstringscalar: requires 1 argument",
                     0, 0, "isstringscalar", "", "m:isstringscalar:nargin");
    outs[0] = isstringscalar(ctx.engine->resource(), args[0]);
}

void isstrprop_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("isstrprop: requires (s, category)",
                     0, 0, "isstrprop", "", "m:isstrprop:nargin");
    outs[0] = isstrprop(ctx.engine->resource(), args[0], args[1]);
}

void isletter_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isletter: requires 1 argument",
                     0, 0, "isletter", "", "m:isletter:nargin");
    outs[0] = isletter(ctx.engine->resource(), args[0]);
}

void isspace_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isspace: requires 1 argument",
                     0, 0, "isspace", "", "m:isspace:nargin");
    outs[0] = isspaceFn(ctx.engine->resource(), args[0]);
}

#define NK_STR_BIN_REG(FN)                                                       \
    void FN##_reg(Span<const Value> args, size_t, Span<Value> outs,             \
                  CallContext &ctx)                                              \
    {                                                                             \
        if (args.size() < 2)                                                      \
            throw Error(#FN " requires 2 arguments",                             \
                         0, 0, #FN, "", "m:" #FN ":nargin");                      \
        outs[0] = FN(ctx.engine->resource(), args[0], args[1]);                  \
    }

NK_STR_BIN_REG(extractAfter)
NK_STR_BIN_REG(extractBefore)

#undef NK_STR_BIN_REG

void extractBetween_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("extractBetween requires (s, start, end)",
                     0, 0, "extractBetween", "", "m:extractBetween:nargin");
    outs[0] = extractBetween(ctx.engine->resource(), args[0], args[1], args[2]);
}

#define NK_STR_TRI_REG(FN)                                                       \
    void FN##_reg(Span<const Value> args, size_t, Span<Value> outs,             \
                  CallContext &ctx)                                              \
    {                                                                             \
        if (args.size() < 3)                                                      \
            throw Error(#FN " requires 3 arguments",                             \
                         0, 0, #FN, "", "m:" #FN ":nargin");                      \
        outs[0] = FN(ctx.engine->resource(), args[0], args[1], args[2]);         \
    }

NK_STR_TRI_REG(insertAfter)
NK_STR_TRI_REG(insertBefore)
NK_STR_TRI_REG(eraseBetween)

#undef NK_STR_TRI_REG

void replaceBetween_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("replaceBetween requires (s, start, end, new)",
                     0, 0, "replaceBetween", "", "m:replaceBetween:nargin");
    outs[0] = replaceBetween(ctx.engine->resource(),
                             args[0], args[1], args[2], args[3]);
}

void dec2bin_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dec2bin requires (d[, n])",
                     0, 0, "dec2bin", "", "m:dec2bin:nargin");
    int n = (args.size() >= 2 && !args[1].isEmpty())
              ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = dec2bin(ctx.engine->resource(), args[0], n);
}

void dec2hex_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dec2hex requires (d[, n])",
                     0, 0, "dec2hex", "", "m:dec2hex:nargin");
    int n = (args.size() >= 2 && !args[1].isEmpty())
              ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = dec2hex(ctx.engine->resource(), args[0], n);
}

void bin2dec_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bin2dec requires 1 argument",
                     0, 0, "bin2dec", "", "m:bin2dec:nargin");
    outs[0] = bin2dec(ctx.engine->resource(), args[0]);
}

void hex2dec_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hex2dec requires 1 argument",
                     0, 0, "hex2dec", "", "m:hex2dec:nargin");
    outs[0] = hex2dec(ctx.engine->resource(), args[0]);
}

void rat_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rat requires (x[, tol])",
                     0, 0, "rat", "", "m:rat:nargin");
    double tol = (args.size() >= 2 && !args[1].isEmpty())
                     ? args[1].toScalar() : 1e-6;
    outs[0] = rat(ctx.engine->resource(), args[0], tol);
}

void rats_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rats requires (x[, len])",
                     0, 0, "rats", "", "m:rats:nargin");
    int len = (args.size() >= 2 && !args[1].isEmpty())
                  ? static_cast<int>(args[1].toScalar()) : 13;
    outs[0] = rats(ctx.engine->resource(), args[0], len);
}

// strtok(s, delim?) — split at first delim char. Returns [token, rem].
// Default delim is ASCII whitespace.
void strtok_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strtok: requires 1 argument", 0, 0, "strtok", "",
                     "m:strtok:nargin");
    const std::string s = args[0].toString();
    const std::string delim = (args.size() >= 2)
                                  ? args[1].toString()
                                  : std::string(" \t\r\n\f\v");
    auto isDelim = [&](char c) { return delim.find(c) != std::string::npos; };

    // Skip leading delim chars.
    size_t start = 0;
    while (start < s.size() && isDelim(s[start])) ++start;
    // Find end of token.
    size_t end = start;
    while (end < s.size() && !isDelim(s[end])) ++end;

    auto *mr = ctx.engine->resource();
    outs[0] = Value::fromString(s.substr(start, end - start), mr);
    if (nargout > 1) {
        outs[1] = Value::fromString(end < s.size() ? s.substr(end) : std::string{}, mr);
    }
}

// ── Pack 36 adapters ─────────────────────────────────────────────────
void newline_reg(Span<const Value> /*args*/, size_t, Span<Value> outs,
                 CallContext &ctx)
{
    outs[0] = newlineFn(ctx.engine->resource());
}

void strings_reg(Span<const Value> args, size_t, Span<Value> outs,
                 CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, args);
    outs[0] = stringsND(mr, d.data(), d.size());
}

void compose_reg(Span<const Value> args, size_t, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("compose: requires 2 arguments (fmt, x)",
                     0, 0, "compose", "", "m:compose:nargin");
    outs[0] = compose(ctx.engine->resource(), args[0], args[1]);
}

void strjust_reg(Span<const Value> args, size_t, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw Error("strjust: requires at least 1 argument",
                     0, 0, "strjust", "", "m:strjust:nargin");
    std::string side = "right";
    if (args.size() >= 2) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("strjust: side must be a char or string",
                         0, 0, "strjust", "", "m:strjust:badSide");
        side = args[1].toString();
    }
    outs[0] = strjust(ctx.engine->resource(), args[0], side);
}

void extract_reg(Span<const Value> args, size_t, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("extract: requires 2 arguments (s, pat)",
                     0, 0, "extract", "", "m:extract:nargin");
    outs[0] = extract(ctx.engine->resource(), args[0], args[1]);
}

void split_reg(Span<const Value> args, size_t, Span<Value> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw Error("split: requires at least 1 argument",
                     0, 0, "split", "", "m:split:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        // Default delimiter is whitespace per MATLAB; we use ' '.
        Value sp = Value::fromString(" ", mr);
        outs[0] = split(mr, args[0], sp);
        return;
    }
    outs[0] = split(mr, args[0], args[1]);
}

void join_reg(Span<const Value> args, size_t, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("join: requires at least 1 argument",
                     0, 0, "join", "", "m:join:nargin");
    if (args.size() >= 2) {
        outs[0] = join(ctx.engine->resource(), args[0], &args[1]);
    } else {
        outs[0] = join(ctx.engine->resource(), args[0], nullptr);
    }
}

} // namespace detail

} // namespace numkit::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — keeps the existing BuiltinLibrary::registerStringFunctions
// hook alive (now empty); actual wiring happens in library.cpp via
// function-pointer adapters, matching Phase-6c pattern.
// ════════════════════════════════════════════════════════════════════════

namespace numkit {

void BuiltinLibrary::registerStringFunctions(Engine &)
{
    // Intentionally empty — all string builtins now register via the
    // Phase-6c function-pointer path in BuiltinLibrary::install().
}

} // namespace numkit
