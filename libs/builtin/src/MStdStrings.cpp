// libs/builtin/src/MStdStrings.cpp

#include <numkit/m/builtin/MStdLibrary.hpp>
#include <numkit/m/builtin/MStdStrings.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

// ── Conversion ──────────────────────────────────────────────────────────

MValue num2str(Allocator &alloc, const MValue &x)
{
    std::ostringstream os;
    os << x.toScalar();
    return MValue::fromString(os.str(), &alloc);
}

MValue str2num(Allocator &alloc, const MValue &s)
{
    try {
        return MValue::scalar(std::stod(s.toString()), &alloc);
    } catch (...) {
        return MValue::empty();
    }
}

MValue str2double(Allocator &alloc, const MValue &s)
{
    try {
        return MValue::scalar(std::stod(s.toString()), &alloc);
    } catch (...) {
        return MValue::scalar(std::numeric_limits<double>::quiet_NaN(), &alloc);
    }
}

MValue toString(Allocator &alloc, const MValue &x)
{
    Allocator *p = &alloc;
    if (x.isString())
        return x;
    if (x.isChar())
        return MValue::stringScalar(x.toString(), p);
    if (x.isNumeric()) {
        if (x.isScalar()) {
            std::ostringstream os;
            os << x.toScalar();
            return MValue::stringScalar(os.str(), p);
        }
        auto result = MValue::stringArray(x.dims().rows(), x.dims().cols());
        for (size_t i = 0; i < x.numel(); ++i) {
            std::ostringstream os;
            os << x.doubleData()[i];
            result.stringElemSet(i, os.str());
        }
        return result;
    }
    if (x.isLogical())
        return MValue::stringScalar(x.toBool() ? "true" : "false", p);
    throw MError("Cannot convert input to string", 0, 0, "string", "",
                 "m:string:unsupportedType");
}

MValue toChar(Allocator &alloc, const MValue &x)
{
    Allocator *p = &alloc;
    if (x.isChar())
        return x;
    if (x.isString())
        return MValue::fromString(x.toString(), p);
    if (x.isNumeric()) {
        std::string s;
        if (x.isScalar()) {
            s += static_cast<char>(static_cast<int>(x.toScalar()));
        } else {
            const double *d = x.doubleData();
            for (size_t i = 0; i < x.numel(); ++i)
                s += static_cast<char>(static_cast<int>(d[i]));
        }
        return MValue::fromString(s, p);
    }
    throw MError("Cannot convert to char", 0, 0, "char", "", "m:char:unsupportedType");
}

// ── Comparisons ─────────────────────────────────────────────────────────

MValue strcmp(Allocator &alloc, const MValue &a, const MValue &b)
{
    return MValue::logicalScalar(a.toString() == b.toString(), &alloc);
}

MValue strcmpi(Allocator &alloc, const MValue &a, const MValue &b)
{
    std::string sa = a.toString(), sb = b.toString();
    std::transform(sa.begin(), sa.end(), sa.begin(), ::tolower);
    std::transform(sb.begin(), sb.end(), sb.begin(), ::tolower);
    return MValue::logicalScalar(sa == sb, &alloc);
}

// ── Case transforms ─────────────────────────────────────────────────────

MValue upper(Allocator &alloc, const MValue &s)
{
    std::string r = s.toString();
    std::transform(r.begin(), r.end(), r.begin(), ::toupper);
    return MValue::fromString(r, &alloc);
}

MValue lower(Allocator &alloc, const MValue &s)
{
    std::string r = s.toString();
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return MValue::fromString(r, &alloc);
}

// ── Trim / split / concat ───────────────────────────────────────────────

MValue strtrim(Allocator &alloc, const MValue &s)
{
    std::string r = s.toString();
    size_t start = r.find_first_not_of(" \t\r\n");
    size_t end = r.find_last_not_of(" \t\r\n");
    if (start == std::string::npos)
        return MValue::fromString("", &alloc);
    return MValue::fromString(r.substr(start, end - start + 1), &alloc);
}

namespace {

MValue strsplitImpl(Allocator &alloc, const std::string &s, char delim)
{
    (void) alloc; // cell construction uses its own default allocation
    std::vector<std::string> parts;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, delim))
        if (!token.empty())
            parts.push_back(token);
    auto c = MValue::cell(1, parts.size());
    for (size_t i = 0; i < parts.size(); ++i)
        c.cellAt(i) = MValue::fromString(parts[i], &alloc);
    return c;
}

} // namespace

MValue strsplit(Allocator &alloc, const MValue &s)
{
    return strsplitImpl(alloc, s.toString(), ' ');
}

MValue strsplit(Allocator &alloc, const MValue &s, const MValue &delim)
{
    std::string d = delim.toString();
    char ch = d.empty() ? ' ' : d[0];
    return strsplitImpl(alloc, s.toString(), ch);
}

MValue strcat(Allocator &alloc, Span<const MValue> parts)
{
    std::string result;
    for (const auto &p : parts)
        result += p.toString();
    return MValue::fromString(result, &alloc);
}

// ── Length ──────────────────────────────────────────────────────────────

MValue strlength(Allocator &alloc, const MValue &s)
{
    Allocator *p = &alloc;
    if (s.isString()) {
        if (s.isScalar())
            return MValue::scalar(static_cast<double>(s.toString().size()), p);
        auto result = createLike(s, MType::DOUBLE, p);
        for (size_t i = 0; i < s.numel(); ++i)
            result.doubleDataMut()[i] = static_cast<double>(s.stringElem(i).size());
        return result;
    }
    if (s.isChar())
        return MValue::scalar(static_cast<double>(s.numel()), p);
    throw MError("Input must be a string or char array", 0, 0, "strlength", "",
                 "m:strlength:unsupportedType");
}

// ── Search / replace ────────────────────────────────────────────────────

MValue strrep(Allocator &alloc, const MValue &s, const MValue &oldPat, const MValue &newPat)
{
    Allocator *p = &alloc;
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
        return MValue::stringScalar(r, p);
    return MValue::fromString(r, p);
}

MValue contains(Allocator &alloc, const MValue &s, const MValue &pat)
{
    std::string ss = s.toString();
    std::string pp = pat.toString();
    return MValue::logicalScalar(ss.find(pp) != std::string::npos, &alloc);
}

MValue startsWith(Allocator &alloc, const MValue &s, const MValue &prefix)
{
    std::string ss = s.toString();
    std::string pp = prefix.toString();
    return MValue::logicalScalar(
        ss.size() >= pp.size() && ss.compare(0, pp.size(), pp) == 0, &alloc);
}

MValue endsWith(Allocator &alloc, const MValue &s, const MValue &suffix)
{
    std::string ss = s.toString();
    std::string pp = suffix.toString();
    return MValue::logicalScalar(
        ss.size() >= pp.size()
            && ss.compare(ss.size() - pp.size(), pp.size(), pp) == 0,
        &alloc);
}

// ── Regex (ECMAScript via std::regex) ────────────────────────────────
namespace {

std::regex compileRegex(const std::string &pat, bool ignoreCase)
{
    auto flags = std::regex::ECMAScript;
    if (ignoreCase) flags |= std::regex::icase;
    try {
        return std::regex(pat, flags);
    } catch (const std::regex_error &e) {
        throw MError(std::string("regex: invalid pattern — ") + e.what(),
                     0, 0, "regexp", "", "m:regexp:badPattern");
    }
}

MValue rowFromIndices(Allocator &alloc, const std::vector<double> &v)
{
    auto out = MValue::matrix(1, v.size(), MType::DOUBLE, &alloc);
    for (std::size_t i = 0; i < v.size(); ++i)
        out.doubleDataMut()[i] = v[i];
    return out;
}

MValue rowCellOfStrings(Allocator &alloc, const std::vector<std::string> &v)
{
    auto out = MValue::cell(1, v.size());
    for (std::size_t i = 0; i < v.size(); ++i)
        out.cellAt(i) = MValue::fromString(v[i], &alloc);
    return out;
}

} // namespace

MValue regexpFind(Allocator &alloc, const MValue &s, const MValue &pat,
                  const std::string &option, bool ignoreCase)
{
    if ((!s.isChar() && !s.isString()) || (!pat.isChar() && !pat.isString()))
        throw MError("regexp: s and pat must be strings",
                     0, 0, "regexp", "", "m:regexp:badArg");
    const std::string text = s.toString();
    const std::regex  re   = compileRegex(pat.toString(), ignoreCase);

    std::string opt = option;
    for (auto &c : opt)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (opt == "split") {
        std::vector<std::string> parts;
        auto begin = std::sregex_iterator(text.begin(), text.end(), re);
        auto end   = std::sregex_iterator();
        std::size_t prev = 0;
        for (auto it = begin; it != end; ++it) {
            const auto &m = *it;
            parts.emplace_back(text.substr(prev, m.position() - prev));
            prev = m.position() + m.length();
        }
        parts.emplace_back(text.substr(prev));
        return rowCellOfStrings(alloc, parts);
    }

    if (opt == "match") {
        std::vector<std::string> matches;
        for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
                  end = std::sregex_iterator(); it != end; ++it)
            matches.emplace_back(it->str());
        return rowCellOfStrings(alloc, matches);
    }

    if (opt == "tokens") {
        // 1×N cell, each entry is a 1×k cell of capture group strings.
        std::vector<std::vector<std::string>> all;
        for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
                  end = std::sregex_iterator(); it != end; ++it) {
            std::vector<std::string> grp;
            for (std::size_t g = 1; g < it->size(); ++g)
                grp.emplace_back(it->str(g));
            all.push_back(std::move(grp));
        }
        auto out = MValue::cell(1, all.size());
        for (std::size_t i = 0; i < all.size(); ++i)
            out.cellAt(i) = rowCellOfStrings(alloc, all[i]);
        return out;
    }

    if (!opt.empty())
        throw MError("regexp: unknown option '" + option
                     + "' (supported: 'match' / 'tokens' / 'split')",
                     0, 0, "regexp", "", "m:regexp:badOption");

    // Default: 1-based start indices.
    std::vector<double> idx;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
              end = std::sregex_iterator(); it != end; ++it)
        idx.push_back(static_cast<double>(it->position() + 1));
    return rowFromIndices(alloc, idx);
}

MValue regexprep(Allocator &alloc, const MValue &s, const MValue &pat,
                 const MValue &rep, bool ignoreCase)
{
    if ((!s.isChar() && !s.isString())
        || (!pat.isChar() && !pat.isString())
        || (!rep.isChar() && !rep.isString()))
        throw MError("regexprep: s, pat, rep must be strings",
                     0, 0, "regexprep", "", "m:regexprep:badArg");
    const std::string text    = s.toString();
    const std::regex  re      = compileRegex(pat.toString(), ignoreCase);
    const std::string repText = rep.toString();
    const std::string out     = std::regex_replace(text, re, repText);
    return MValue::fromString(out, &alloc);
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void num2str_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("num2str: requires 1 argument", 0, 0, "num2str", "", "m:num2str:nargin");
    outs[0] = num2str(ctx.engine->allocator(), args[0]);
}

void str2num_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("str2num: requires 1 argument", 0, 0, "str2num", "", "m:str2num:nargin");
    outs[0] = str2num(ctx.engine->allocator(), args[0]);
}

void str2double_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("str2double: requires 1 argument", 0, 0, "str2double", "",
                     "m:str2double:nargin");
    outs[0] = str2double(ctx.engine->allocator(), args[0]);
}

void string_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    Allocator &alloc = ctx.engine->allocator();
    if (args.empty()) {
        outs[0] = MValue::stringScalar("", &alloc);
        return;
    }
    outs[0] = toString(alloc, args[0]);
}

void char_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("char requires an argument", 0, 0, "char", "", "m:char:nargin");
    outs[0] = toChar(ctx.engine->allocator(), args[0]);
}

void strcmp_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("strcmp: requires 2 arguments", 0, 0, "strcmp", "", "m:strcmp:nargin");
    outs[0] = strcmp(ctx.engine->allocator(), args[0], args[1]);
}

void strcmpi_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("strcmpi: requires 2 arguments", 0, 0, "strcmpi", "",
                     "m:strcmpi:nargin");
    outs[0] = strcmpi(ctx.engine->allocator(), args[0], args[1]);
}

void upper_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("upper: requires 1 argument", 0, 0, "upper", "", "m:upper:nargin");
    outs[0] = upper(ctx.engine->allocator(), args[0]);
}

void lower_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("lower: requires 1 argument", 0, 0, "lower", "", "m:lower:nargin");
    outs[0] = lower(ctx.engine->allocator(), args[0]);
}

void strtrim_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("strtrim: requires 1 argument", 0, 0, "strtrim", "",
                     "m:strtrim:nargin");
    outs[0] = strtrim(ctx.engine->allocator(), args[0]);
}

void strsplit_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("strsplit: requires 1 argument", 0, 0, "strsplit", "",
                     "m:strsplit:nargin");
    if (args.size() == 1)
        outs[0] = strsplit(ctx.engine->allocator(), args[0]);
    else
        outs[0] = strsplit(ctx.engine->allocator(), args[0], args[1]);
}

void strcat_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    outs[0] = strcat(ctx.engine->allocator(), args);
}

void strlength_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("strlength: requires 1 argument", 0, 0, "strlength", "",
                     "m:strlength:nargin");
    outs[0] = strlength(ctx.engine->allocator(), args[0]);
}

void strrep_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw MError("strrep requires 3 arguments", 0, 0, "strrep", "", "m:strrep:nargin");
    outs[0] = strrep(ctx.engine->allocator(), args[0], args[1], args[2]);
}

void contains_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("contains requires 2 arguments", 0, 0, "contains", "",
                     "m:contains:nargin");
    outs[0] = contains(ctx.engine->allocator(), args[0], args[1]);
}

void startsWith_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("startsWith requires 2 arguments", 0, 0, "startsWith", "",
                     "m:startsWith:nargin");
    outs[0] = startsWith(ctx.engine->allocator(), args[0], args[1]);
}

void endsWith_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("endsWith requires 2 arguments", 0, 0, "endsWith", "",
                     "m:endsWith:nargin");
    outs[0] = endsWith(ctx.engine->allocator(), args[0], args[1]);
}

void regexp_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("regexp: requires at least 2 arguments (s, pat)",
                     0, 0, "regexp", "", "m:regexp:nargin");
    std::string opt;
    if (args.size() >= 3) {
        if (!args[2].isChar() && !args[2].isString())
            throw MError("regexp: option must be a string",
                         0, 0, "regexp", "", "m:regexp:badOption");
        opt = args[2].toString();
    }
    outs[0] = regexpFind(ctx.engine->allocator(), args[0], args[1], opt, false);
}

void regexpi_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("regexpi: requires at least 2 arguments (s, pat)",
                     0, 0, "regexpi", "", "m:regexpi:nargin");
    std::string opt;
    if (args.size() >= 3) {
        if (!args[2].isChar() && !args[2].isString())
            throw MError("regexpi: option must be a string",
                         0, 0, "regexpi", "", "m:regexpi:badOption");
        opt = args[2].toString();
    }
    outs[0] = regexpFind(ctx.engine->allocator(), args[0], args[1], opt, true);
}

void regexprep_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw MError("regexprep: requires 3 arguments (s, pat, rep)",
                     0, 0, "regexprep", "", "m:regexprep:nargin");
    outs[0] = regexprep(ctx.engine->allocator(), args[0], args[1], args[2], false);
}

} // namespace detail

} // namespace numkit::m::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — keeps the existing StdLibrary::registerStringFunctions
// hook alive (now empty); actual wiring happens in MStdLibrary.cpp via
// function-pointer adapters, matching Phase-6c pattern.
// ════════════════════════════════════════════════════════════════════════

namespace numkit::m {

void StdLibrary::registerStringFunctions(Engine &)
{
    // Intentionally empty — all string builtins now register via the
    // Phase-6c function-pointer path in StdLibrary::install().
}

} // namespace numkit::m
