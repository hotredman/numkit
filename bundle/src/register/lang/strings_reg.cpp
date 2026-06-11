// toolboxes/builtin/src/language/strings/strings_reg.cpp
//
// CallContext register half (Phase 2b multi-block split).
#include <numkit/core/engine.hpp>
#include <numkit/lang/strings/format.hpp>
#include <numkit/lang/strings/strings.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include <numkit/ops/helpers.hpp>
#include "strings/strings_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::builtin {
using namespace numkit::lang;  // C4c localized (umbrella removed)

namespace detail {

void num2str_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("num2str: requires 1 argument", 0, 0, "num2str", "", "numkit:num2str:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() < 2) {
        outs[0] = num2str(args[0], mr);
        return;
    }
    const Value &spec = args[1];
    if (spec.isChar() || spec.isString())
        outs[0] = num2str(args[0], spec.toString(), mr);
    else
        outs[0] = num2str(args[0], static_cast<int>(spec.toScalar()), mr);
}

void int2str_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("int2str: requires 1 argument", 0, 0, "int2str", "", "numkit:int2str:nargin");
    outs[0] = int2str(args[0], ctx.engine->resource());
}

void validatestring_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    // validatestring(str, validStrings [, funcName, varName, position]).
    // The trailing args only customise the error text; they don't change the
    // match, so we accept and ignore them.
    if (args.size() < 2)
        throw Error("validatestring: requires at least 2 arguments (str, validStrings)",
                     0, 0, "validatestring", "", "numkit:validatestring:nargin");
    outs[0] = validatestring(args[0], args[1], ctx.engine->resource());
}

void str2num_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("str2num: requires 1 argument", 0, 0, "str2num", "", "numkit:str2num:nargin");
    // MATLAB str2num evaluates the (bracket-wrapped) string as an expression,
    // so it parses matrices, ranges and arithmetic — str2num('[1 2;3 4]')=
    // [1 2;3 4], '1:5'=1..5, '2+3'=5. Any parse/eval failure (or a non-numeric
    // result) yields [] (0x0 double); the optional 2nd output is a logical
    // success flag: [X, tf] = str2num(s). (The engine-free Value str2num()
    // overload remains a scalar-only fallback for embedders without eval.)
    bool ok = false;
    Value result = Value::Empty;
    if (args[0].isChar() || args[0].isString()) {
        try {
            Value v = ctx.engine->eval("[" + args[0].toString() + "]",
                                       /*suppressTopLevelDisplay=*/true);
            if (v.isNumeric() || v.isLogical()) {
                result = std::move(v);
                ok = true;
            }
        } catch (...) {
            ok = false;
        }
    }
    outs[0] = ok ? std::move(result) : Value::Empty;
    if (nargout >= 2 && outs.size() >= 2)
        outs[1] = Value::logicalScalar(ok, ctx.engine->resource());
}

void str2double_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("str2double: requires 1 argument", 0, 0, "str2double", "",
                     "numkit:str2double:nargin");
    outs[0] = str2double(args[0], ctx.engine->resource());
}

void string_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::stringScalar("", mr);
        return;
    }
    outs[0] = toString(args[0], mr);
}

void char_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("char requires an argument", 0, 0, "char", "", "numkit:char:nargin");
    outs[0] = toChar(args[0], ctx.engine->resource());
}

void strcmp_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("strcmp: requires 2 arguments", 0, 0, "strcmp", "", "numkit:strcmp:nargin");
    outs[0] = strcmp(args[0], args[1], ctx.engine->resource());
}

void strcmpi_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("strcmpi: requires 2 arguments", 0, 0, "strcmpi", "",
                     "numkit:strcmpi:nargin");
    outs[0] = strcmpi(args[0], args[1], ctx.engine->resource());
}

void upper_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("upper: requires 1 argument", 0, 0, "upper", "", "numkit:upper:nargin");
    outs[0] = upper(args[0], ctx.engine->resource());
}

void lower_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("lower: requires 1 argument", 0, 0, "lower", "", "numkit:lower:nargin");
    outs[0] = lower(args[0], ctx.engine->resource());
}

void strtrim_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strtrim: requires 1 argument", 0, 0, "strtrim", "",
                     "numkit:strtrim:nargin");
    outs[0] = strtrim(args[0], ctx.engine->resource());
}

void strsplit_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strsplit: requires 1 argument", 0, 0, "strsplit", "",
                     "numkit:strsplit:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    ScratchVec<std::string> delims(&scratch);
    bool collapse = true;
    // arg 1 (optional) is the delimiter: a string or cell array of strings.
    // Name-Value pairs (CollapseDelimiters) follow at arg 2+.
    size_t optStart = 1;
    if (args.size() >= 2) {
        appendDelims(args[1], delims);
        optStart = 2;
    } else {
        appendDefaultWhitespace(delims);
    }
    for (size_t k = optStart; k + 1 < args.size(); k += 2) {
        std::string name = args[k].toString();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char ch) { return std::tolower(ch); });
        if (name == "collapsedelimiters")
            collapse = !args[k + 1].isEmpty() && args[k + 1].toScalar() != 0.0;
        // DelimiterType='RegularExpression' is not supported (literal only).
    }
    if (nargout >= 2) {
        // [tokens, matches] = strsplit(...): also return the matched
        // delimiters (1×(numel(tokens)-1) cell of strings).
        ScratchVec<std::string> matched(&scratch);
        outs[0] = strsplitImpl(args[0].toString(), delims, collapse, mr, &matched);
        auto mc = Value::cell(1, matched.size());
        for (size_t k = 0; k < matched.size(); ++k)
            mc.cellAt(k) = Value::fromString(matched[k], mr);
        outs[1] = mc;
    } else {
        outs[0] = strsplitImpl(args[0].toString(), delims, collapse, mr);
    }
}

void strcat_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    outs[0] = strcat(args, ctx.engine->resource());
}

void strlength_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strlength: requires 1 argument", 0, 0, "strlength", "",
                     "numkit:strlength:nargin");
    outs[0] = strlength(args[0], ctx.engine->resource());
}

void strrep_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("strrep requires 3 arguments", 0, 0, "strrep", "", "numkit:strrep:nargin");
    outs[0] = strrep(args[0], args[1], args[2], ctx.engine->resource());
}

// Parse a trailing 'IgnoreCase', <logical> name-value pair (MATLAB option
// shared by contains/startsWith/endsWith). Other name-value pairs are left
// alone. Returns the flag; absent → false.
static bool parseIgnoreCaseOption(Span<const Value> args, size_t optStart)
{
    for (size_t i = optStart; i + 1 < args.size(); ++i) {
        if (!args[i].isChar() && !args[i].isString()) continue;
        std::string k = args[i].toString();
        for (char &c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (k == "ignorecase")
            return args[i + 1].toBool();
    }
    return false;
}

void contains_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("contains requires 2 arguments", 0, 0, "contains", "",
                     "numkit:contains:nargin");
    outs[0] = contains(args[0], args[1], parseIgnoreCaseOption(args, 2),
                       ctx.engine->resource());
}

void startsWith_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("startsWith requires 2 arguments", 0, 0, "startsWith", "",
                     "numkit:startsWith:nargin");
    outs[0] = startsWith(args[0], args[1], parseIgnoreCaseOption(args, 2),
                         ctx.engine->resource());
}

void endsWith_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("endsWith requires 2 arguments", 0, 0, "endsWith", "",
                     "numkit:endsWith:nargin");
    outs[0] = endsWith(args[0], args[1], parseIgnoreCaseOption(args, 2),
                       ctx.engine->resource());
}

void strncmp_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("strncmp: requires 3 arguments", 0, 0, "strncmp", "",
                     "numkit:strncmp:nargin");
    const size_t n = static_cast<size_t>(args[2].toScalar());
    outs[0] = strncmp(args[0], args[1], n, ctx.engine->resource());
}

void strncmpi_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("strncmpi: requires 3 arguments", 0, 0, "strncmpi", "",
                     "numkit:strncmpi:nargin");
    const size_t n = static_cast<size_t>(args[2].toScalar());
    outs[0] = strncmpi(args[0], args[1], n, ctx.engine->resource());
}

void strfind_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("strfind: requires 2 arguments", 0, 0, "strfind", "",
                     "numkit:strfind:nargin");
    outs[0] = strfind(args[0], args[1], ctx.engine->resource());
}

void blanks_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("blanks: requires 1 argument", 0, 0, "blanks", "",
                     "numkit:blanks:nargin");
    const size_t n = static_cast<size_t>(args[0].toScalar());
    outs[0] = blanks(n, ctx.engine->resource());
}

void deblank_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("deblank: requires 1 argument", 0, 0, "deblank", "",
                     "numkit:deblank:nargin");
    outs[0] = deblank(args[0], ctx.engine->resource());
}

void mat2str_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mat2str: requires at least 1 argument", 0, 0, "mat2str", "",
                     "numkit:mat2str:nargin");
    auto *mr = ctx.engine->resource();
    int prec = 15;
    bool withClass = false;
    // Trailing args (in any order): a numeric precision, and/or the literal
    // 'class' flag which wraps the output with the class name
    // (mat2str(int8([1 2]),'class') -> "int8([1 2])").
    for (size_t k = 1; k < args.size(); ++k) {
        const Value &a = args[k];
        if (a.type() == ValueType::CHAR || a.isString()) {
            std::string s = a.toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char ch) { return std::tolower(ch); });
            if (s == "class") withClass = true;
        } else if (!a.isEmpty()) {
            prec = static_cast<int>(a.toScalar());
        }
    }
    Value r = mat2str(args[0], prec, mr);
    if (withClass) {
        std::string wrapped = std::string(mtypeName(args[0].type())) + "(" +
                              r.toString() + ")";
        r = Value::fromString(wrapped, mr);
    }
    outs[0] = r;
}

void strjoin_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strjoin: requires at least 1 argument", 0, 0, "strjoin", "",
                     "numkit:strjoin:nargin");
    const Value &delim = (args.size() >= 2) ? args[1] : Value::Empty;
    outs[0] = strjoin(args[0], delim, ctx.engine->resource());
}

void append_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    outs[0] = append(args, ctx.engine->resource());
}

void count_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("count: requires (s, pat)",
                     0, 0, "count", "", "numkit:count:nargin");
    outs[0] = count(args[0], args[1], ctx.engine->resource());
}

void erase_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("erase: requires (s, pat)",
                     0, 0, "erase", "", "numkit:erase:nargin");
    outs[0] = erase(args[0], args[1], ctx.engine->resource());
}

void replace_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("replace: requires (s, old, new)",
                     0, 0, "replace", "", "numkit:replace:nargin");
    outs[0] = replace(args[0], args[1], args[2], ctx.engine->resource());
}

void reverse_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("reverse: requires 1 argument",
                     0, 0, "reverse", "", "numkit:reverse:nargin");
    outs[0] = reverse(args[0], ctx.engine->resource());
}

void splitlines_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("splitlines: requires 1 argument",
                     0, 0, "splitlines", "", "numkit:splitlines:nargin");
    outs[0] = splitlines(args[0], ctx.engine->resource());
}

// Default pad width: the longest element of a cell str, or the length of a
// char/string scalar (so pad(s) with no width is a no-op for a scalar but
// right-pads every cell element to the longest, matching MATLAB).
static size_t defaultPadWidth(const Value &s)
{
    if (s.isCell()) {
        size_t mx = 0;
        const size_t nn = s.numel();
        for (size_t i = 0; i < nn; ++i)
            mx = std::max(mx, s.cellAt(i).toString().size());
        return mx;
    }
    if (s.isString()) {
        // A string array defaults to the longest element (s.toString() would
        // collapse the array to its first element).
        size_t mx = 0;
        const size_t nn = s.numel();
        for (size_t i = 0; i < nn; ++i)
            mx = std::max(mx, s.stringElem(i).size());
        return mx;
    }
    return s.toString().size();
}

void pad_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pad: requires (s[, n][, side[, ch]])",
                     0, 0, "pad", "", "numkit:pad:nargin");
    // The 2nd arg is the width n when numeric; a string there is the side
    // (with the width defaulting to the longest element). pad(s) with no
    // 2nd arg also uses the default width.
    const bool haveN = args.size() >= 2 && !args[1].isEmpty()
                       && !args[1].isChar() && !args[1].isString();
    size_t n, sideIdx, chIdx;
    if (haveN) {
        n = static_cast<size_t>(args[1].toScalar());
        sideIdx = 2;
        chIdx = 3;
    } else {
        n = defaultPadWidth(args[0]);
        sideIdx = 1;   // a string 2nd arg is the side
        chIdx = 2;
    }
    const Value &side = (args.size() > sideIdx && !args[sideIdx].isEmpty()) ? args[sideIdx] : Value::Empty;
    const Value &ch   = (args.size() > chIdx   && !args[chIdx].isEmpty())   ? args[chIdx]   : Value::Empty;
    outs[0] = pad(args[0], n, side, ch, ctx.engine->resource());
}

void strip_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strip: requires (s[, side[, ch]])",
                     0, 0, "strip", "", "numkit:strip:nargin");
    const Value &side = (args.size() >= 2 && !args[1].isEmpty()) ? args[1] : Value::Empty;
    const Value &ch   = (args.size() >= 3 && !args[2].isEmpty()) ? args[2] : Value::Empty;
    outs[0] = strip(args[0], side, ch, ctx.engine->resource());
}

void matches_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("matches: requires (s, pat)",
                     0, 0, "matches", "", "numkit:matches:nargin");
    outs[0] = matches(args[0], args[1], ctx.engine->resource());
}

void convertCharsToStrings_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("convertCharsToStrings: requires 1 argument",
                     0, 0, "convertCharsToStrings", "", "numkit:convertCharsToStrings:nargin");
    outs[0] = convertCharsToStrings(args[0], ctx.engine->resource());
}

void convertStringsToChars_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("convertStringsToChars: requires 1 argument",
                     0, 0, "convertStringsToChars", "", "numkit:convertStringsToChars:nargin");
    outs[0] = convertStringsToChars(args[0], ctx.engine->resource());
}

void isstringscalar_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isstringscalar: requires 1 argument",
                     0, 0, "isstringscalar", "", "numkit:isstringscalar:nargin");
    outs[0] = isstringscalar(args[0], ctx.engine->resource());
}

void isstrprop_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("isstrprop: requires (s, category)",
                     0, 0, "isstrprop", "", "numkit:isstrprop:nargin");
    outs[0] = isstrprop(args[0], args[1], ctx.engine->resource());
}

void isletter_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isletter: requires 1 argument",
                     0, 0, "isletter", "", "numkit:isletter:nargin");
    outs[0] = isletter(args[0], ctx.engine->resource());
}

void isspace_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isspace: requires 1 argument",
                     0, 0, "isspace", "", "numkit:isspace:nargin");
    outs[0] = isspaceFn(args[0], ctx.engine->resource());
}

#define NK_STR_BIN_REG(FN)                                                       \
    void FN##_reg(Span<const Value> args, size_t, Span<Value> outs,             \
                  CallContext &ctx)                                              \
    {                                                                             \
        if (args.size() < 2)                                                      \
            throw Error(#FN " requires 2 arguments",                             \
                         0, 0, #FN, "", "numkit:" #FN ":nargin");                      \
        outs[0] = FN(args[0], args[1], ctx.engine->resource());                  \
    }

NK_STR_BIN_REG(extractAfter)
NK_STR_BIN_REG(extractBefore)

#undef NK_STR_BIN_REG

void extractBetween_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("extractBetween requires (s, start, end)",
                     0, 0, "extractBetween", "", "numkit:extractBetween:nargin");
    outs[0] = extractBetween(args[0], args[1], args[2], ctx.engine->resource());
}

#define NK_STR_TRI_REG(FN)                                                       \
    void FN##_reg(Span<const Value> args, size_t, Span<Value> outs,             \
                  CallContext &ctx)                                              \
    {                                                                             \
        if (args.size() < 3)                                                      \
            throw Error(#FN " requires 3 arguments",                             \
                         0, 0, #FN, "", "numkit:" #FN ":nargin");                      \
        outs[0] = FN(args[0], args[1], args[2], ctx.engine->resource());         \
    }

NK_STR_TRI_REG(insertAfter)
NK_STR_TRI_REG(insertBefore)
NK_STR_TRI_REG(eraseBetween)

#undef NK_STR_TRI_REG

void replaceBetween_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("replaceBetween requires (s, start, end, new)",
                     0, 0, "replaceBetween", "", "numkit:replaceBetween:nargin");
    outs[0] = replaceBetween(args[0], args[1], args[2], args[3], ctx.engine->resource());
}

void dec2bin_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dec2bin requires (d[, n])",
                     0, 0, "dec2bin", "", "numkit:dec2bin:nargin");
    int n = (args.size() >= 2 && !args[1].isEmpty())
              ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = dec2bin(args[0], n, ctx.engine->resource());
}

void dec2hex_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dec2hex requires (d[, n])",
                     0, 0, "dec2hex", "", "numkit:dec2hex:nargin");
    int n = (args.size() >= 2 && !args[1].isEmpty())
              ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = dec2hex(args[0], n, ctx.engine->resource());
}

void bin2dec_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bin2dec requires 1 argument",
                     0, 0, "bin2dec", "", "numkit:bin2dec:nargin");
    outs[0] = bin2dec(args[0], ctx.engine->resource());
}

void hex2dec_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hex2dec requires 1 argument",
                     0, 0, "hex2dec", "", "numkit:hex2dec:nargin");
    outs[0] = hex2dec(args[0], ctx.engine->resource());
}

void hex2num_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hex2num requires 1 argument",
                     0, 0, "hex2num", "", "numkit:hex2num:nargin");
    outs[0] = hex2num(args[0], ctx.engine->resource());
}

void num2hex_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("num2hex requires 1 argument",
                     0, 0, "num2hex", "", "numkit:num2hex:nargin");
    outs[0] = num2hex(args[0], ctx.engine->resource());
}

void dec2base_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dec2base requires (d, base[, len])",
                     0, 0, "dec2base", "", "numkit:dec2base:nargin");
    const int base = static_cast<int>(args[1].toScalar());
    const int len  = (args.size() >= 3 && !args[2].isEmpty())
                       ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = dec2base(args[0], base, len, ctx.engine->resource());
}

void base2dec_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("base2dec requires (s, base)",
                     0, 0, "base2dec", "", "numkit:base2dec:nargin");
    const int base = static_cast<int>(args[1].toScalar());
    outs[0] = base2dec(args[0], base, ctx.engine->resource());
}

void rat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rat requires (x[, tol])",
                     0, 0, "rat", "", "numkit:rat:nargin");
    auto *mr = ctx.engine->resource();
    const Value &x = args[0];
    const double user_tol = (args.size() >= 2 && !args[1].isEmpty())
                            ? args[1].toScalar() : -1.0;  // -1 ⇒ defaultRatTol per element

    if (nargout <= 1) {
        // 1-output form: continued-fraction string. MATLAB's vector
        // version returns a multi-line char array; numkit only emits
        // the scalar form for now (matrix-of-strings is a separate
        // edge — never hit by realistic call sites).
        outs[0] = rat(x, user_tol > 0 ? user_tol : 0.0, mr);
        return;
    }

    // 2-output form: [N, D] = rat(x[, tol]) — numeric, vectorised.
    const auto &dims = x.dims();
    const size_t n = x.numel();
    Value N = dims.is3D()
        ? Value::matrix3d(dims.rows(), dims.cols(), dims.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(dims.rows(), dims.cols(), ValueType::DOUBLE, mr);
    Value D = dims.is3D()
        ? Value::matrix3d(dims.rows(), dims.cols(), dims.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(dims.rows(), dims.cols(), ValueType::DOUBLE, mr);
    double *Nd = N.doubleDataMut();
    double *Dd = D.doubleDataMut();
    for (size_t i = 0; i < n; ++i) {
        const double v = x.elemAsDouble(i);
        if (!std::isfinite(v)) {
            // MATLAB returns NaN/NaN for NaN input, ±Inf/0 for ±Inf? Empirically
            // [n, d] = rat(NaN) → n=NaN, d=NaN; [n, d] = rat(Inf) → n=Inf, d=1.
            if (std::isnan(v)) {
                Nd[i] = std::numeric_limits<double>::quiet_NaN();
                Dd[i] = std::numeric_limits<double>::quiet_NaN();
            } else {
                Nd[i] = v;  // ±Inf
                Dd[i] = 1.0;
            }
            continue;
        }
        const double tol = (user_tol > 0) ? user_tol : defaultRatTol(v);
        const auto exp = ratExpansion(v, tol);
        Nd[i] = static_cast<double>(exp.p);
        Dd[i] = static_cast<double>(exp.q);
    }
    outs[0] = std::move(N);
    outs[1] = std::move(D);
}

void rats_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rats requires (x[, len])",
                     0, 0, "rats", "", "numkit:rats:nargin");
    int len = (args.size() >= 2 && !args[1].isEmpty())
                  ? static_cast<int>(args[1].toScalar()) : 13;
    outs[0] = rats(args[0], len, ctx.engine->resource());
}

// strtok(s, delim?) — split at first delim char. Returns [token, rem].
// Default delim is ASCII whitespace.
void strtok_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strtok: requires 1 argument", 0, 0, "strtok", "",
                     "numkit:strtok:nargin");
    const std::string delim = (args.size() >= 2)
                                  ? args[1].toString()
                                  : std::string(" \t\r\n\f\v");
    auto [tok, rem] = numkit::lang::strtok(args[0], delim,
                                              ctx.engine->resource());
    outs[0] = std::move(tok);
    if (nargout > 1) outs[1] = std::move(rem);
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
    outs[0] = stringsND(Span<const size_t>(d.data(), d.size()), mr);
}

void compose_reg(Span<const Value> args, size_t, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("compose: requires 2 arguments (fmt, x)",
                     0, 0, "compose", "", "numkit:compose:nargin");
    outs[0] = compose(args[0], args[1], ctx.engine->resource());
}

void strjust_reg(Span<const Value> args, size_t, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw Error("strjust: requires at least 1 argument",
                     0, 0, "strjust", "", "numkit:strjust:nargin");
    std::string side = "right";
    if (args.size() >= 2) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("strjust: side must be a char or string",
                         0, 0, "strjust", "", "numkit:strjust:badSide");
        side = args[1].toString();
    }
    outs[0] = strjust(args[0], side, ctx.engine->resource());
}

void extract_reg(Span<const Value> args, size_t, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("extract: requires 2 arguments (s, pat)",
                     0, 0, "extract", "", "numkit:extract:nargin");
    outs[0] = extract(args[0], args[1], ctx.engine->resource());
}

void split_reg(Span<const Value> args, size_t, Span<Value> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw Error("split: requires at least 1 argument",
                     0, 0, "split", "", "numkit:split:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        // Default delimiter is whitespace per MATLAB; we use ' '.
        Value sp = Value::fromString(" ", mr);
        outs[0] = split(args[0], sp, mr);
        return;
    }
    outs[0] = split(args[0], args[1], mr);
}

void join_reg(Span<const Value> args, size_t, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("join: requires at least 1 argument",
                     0, 0, "join", "", "numkit:join:nargin");
    const Value &delim = (args.size() >= 2) ? args[1] : Value::Empty;
    outs[0] = join(args[0], delim, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
