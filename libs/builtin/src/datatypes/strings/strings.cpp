// libs/builtin/src/datatypes/strings/strings.cpp
//
// Non-regex string builtins: num2str / str2num / str2double / string /
// char / strcmp / strcmpi / upper / lower / strtrim / strsplit / strcat /
// strlength / strrep / contains / startsWith / endsWith. The regex
// builtins (regexp/regexpi/regexprep) live in regex.cpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/datatypes/strings/strings.hpp>

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
