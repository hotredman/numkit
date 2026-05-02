// libs/builtin/src/datatypes/strings/strings.cpp
//
// Non-regex string builtins: num2str / str2num / str2double / string /
// char / strcmp / strcmpi / upper / lower / strtrim / strsplit / strcat /
// strlength / strrep / contains / startsWith / endsWith. The regex
// builtins (regexp/regexpi/regexprep) live in regex.cpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/language/strings/strings.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>
#include <string>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

// ── Conversion ──────────────────────────────────────────────────────────

Value num2str(std::pmr::memory_resource *mr, const Value &x)
{
    std::ostringstream os;
    os << x.toScalar();
    return Value::fromString(os.str(), mr);
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

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void num2str_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("num2str: requires 1 argument", 0, 0, "num2str", "", "m:num2str:nargin");
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
