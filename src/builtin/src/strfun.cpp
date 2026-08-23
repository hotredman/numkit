// src/builtin/src/strfun.cpp
//
// String and character array manipulation implementations and registrations.
#include <numkit/builtin/strfun.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/lang/strings/strings.hpp>
#include <numkit/lang/strings/regex.hpp>

#include <stdexcept>
#include <string>

namespace numkit::builtin {

namespace detail {
void append_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void base2dec_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bin2dec_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void blanks_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void char_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void compose_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void contains_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void convertCharsToStrings_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void convertStringsToChars_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void count_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void deblank_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void dec2base_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void dec2bin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void dec2hex_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void endsWith_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erase_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void eraseBetween_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void extract_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void extractAfter_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void extractBefore_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void extractBetween_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void hex2dec_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void hex2num_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void insertAfter_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void insertBefore_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int2str_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isletter_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isspace_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isstringscalar_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isstrprop_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void join_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void lower_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mat2str_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void matches_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void newline_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void num2hex_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void num2str_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pad_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rats_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void regexp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void regexpi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void regexprep_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void regexptranslate_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void replace_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void replaceBetween_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void reverse_reg(Span<const Value> args, size_t, Span<Value>, CallContext&);
void split_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void splitlines_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void startsWith_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void str2double_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void str2num_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strcat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strcmp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strcmpi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strfind_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void string_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strings_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strip_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strjoin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strjust_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strlength_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strncmp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strncmpi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strrep_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strsplit_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strtok_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strtrim_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void upper_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void validatestring_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
} // namespace detail

// ── Pure C++ API Implementations ───────────────────────────────────────────

Value num2str(const Value &x, std::pmr::memory_resource *mr) { return numkit::lang::num2str(x, mr); }
Value int2str(const Value &x, std::pmr::memory_resource *mr) { return numkit::lang::int2str(x, mr); }
Value mat2str(const Value &x, std::pmr::memory_resource *mr) { return numkit::lang::mat2str(x, 15, mr); }
Value str2num(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::str2num(s, mr); }
Value str2double(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::str2double(s, mr); }
Value char_array(const Value &x, std::pmr::memory_resource *mr) { return numkit::lang::toChar(x, mr); }
Value string_array(const Value &x, std::pmr::memory_resource *mr) { return numkit::lang::toString(x, mr); }
Value blanks(size_t n, std::pmr::memory_resource *mr) { return numkit::lang::blanks(n, mr); }
Value newline(std::pmr::memory_resource *mr) { return Value::fromString("\n", mr); }

Value strcmp(const Value &s1, const Value &s2, std::pmr::memory_resource *mr) { return numkit::lang::strcmp(s1, s2, mr); }
Value strncmp(const Value &s1, const Value &s2, size_t n, std::pmr::memory_resource *mr) { return numkit::lang::strncmp(s1, s2, n, mr); }
Value strcmpi(const Value &s1, const Value &s2, std::pmr::memory_resource *mr) { return numkit::lang::strcmpi(s1, s2, mr); }
Value strncmpi(const Value &s1, const Value &s2, size_t n, std::pmr::memory_resource *mr) { return numkit::lang::strncmpi(s1, s2, n, mr); }
Value matches(const Value &str, const Value &pat, std::pmr::memory_resource *mr) { return numkit::lang::matches(str, pat, mr); }

Value upper(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::upper(s, mr); }
Value lower(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::lower(s, mr); }
Value strtrim(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::strtrim(s, mr); }
Value deblank(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::deblank(s, mr); }
Value strip(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::strip(s, Value::Empty, Value::Empty, mr); }

Value strcat(Span<const Value> args, std::pmr::memory_resource *mr) { return numkit::lang::strcat(args, mr); }
Value strjoin(const Value &c, const std::string &delim, std::pmr::memory_resource *mr) { return numkit::lang::strjoin(c, delim.empty() ? Value::Empty : Value::fromString(delim, mr), mr); }
Value strsplit(const Value &s, const std::string &delim, std::pmr::memory_resource *mr) { return delim.empty() ? numkit::lang::strsplit(s, mr) : numkit::lang::strsplit(s, Value::fromString(delim, mr), mr); }
Value splitlines(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::splitlines(s, mr); }

Value strfind(const Value &text, const Value &pattern, std::pmr::memory_resource *mr) { return numkit::lang::strfind(text, pattern, mr); }
Value strrep(const Value &orig, const Value &oldStr, const Value &newStr, std::pmr::memory_resource *mr) { return numkit::lang::strrep(orig, oldStr, newStr, mr); }
Value contains(const Value &str, const Value &pat, std::pmr::memory_resource *mr) { return numkit::lang::contains(str, pat, mr); }
Value startsWith(const Value &str, const Value &pat, std::pmr::memory_resource *mr) { return numkit::lang::startsWith(str, pat, mr); }
Value endsWith(const Value &str, const Value &pat, std::pmr::memory_resource *mr) { return numkit::lang::endsWith(str, pat, mr); }
Value count(const Value &str, const Value &pat, std::pmr::memory_resource *mr) { return numkit::lang::count(str, pat, mr); }
Value reverse(const Value &str, std::pmr::memory_resource *mr) { return numkit::lang::reverse(str, mr); }

Value isletter(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::isletter(s, mr); }
Value isspace(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::isspaceFn(s, mr); }
Value isstrprop(const Value &s, const std::string &category, std::pmr::memory_resource *mr) { return numkit::lang::isstrprop(s, Value::fromString(category, mr), mr); }
Value isstringscalar(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::isstringscalar(s, mr); }
Value validatestring(const Value &str, const Value &validStrings, std::pmr::memory_resource *mr) { return numkit::lang::validatestring(str, validStrings, mr); }

Value convertContainedStringsToChars(const Value &v, std::pmr::memory_resource *mr)
{
    if (v.isString()) {
        if (v.numel() <= 1)
            return Value::fromString(v.toString(), mr);
        auto c = Value::cell(v.numel(), 1, mr);
        for (size_t i = 0; i < v.numel(); ++i)
            c.cellAt(i) = Value::fromString(v.stringElem(i), mr);
        return c;
    }
    if (v.isCell()) {
        const auto &d = v.dims();
        auto c = d.is3D()
                    ? Value::cell3D(d.rows(), d.cols(), d.pages(), mr)
                    : Value::cell(d.rows(), d.cols(), mr);
        for (size_t i = 0; i < v.numel(); ++i)
            c.cellAt(i) = convertContainedStringsToChars(v.cellAt(i), mr);
        return c;
    }
    if (v.isStruct() && !v.isStructArray()) {
        auto s = Value::structure(mr);
        for (auto &kv : v.structFields())
            s.field(kv.first) = convertContainedStringsToChars(kv.second, mr);
        return s;
    }
    return v;
}

Value dec2bin(const Value &d, int minDigits, std::pmr::memory_resource *mr) { return numkit::lang::dec2bin(d, minDigits, mr); }
Value dec2hex(const Value &d, int minDigits, std::pmr::memory_resource *mr) { return numkit::lang::dec2hex(d, minDigits, mr); }
Value dec2base(const Value &d, int base, int minDigits, std::pmr::memory_resource *mr) { return numkit::lang::dec2base(d, base, minDigits, mr); }
Value base2dec(const Value &s, int base, std::pmr::memory_resource *mr) { return numkit::lang::base2dec(s, base, mr); }
Value bin2dec(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::bin2dec(s, mr); }
Value hex2dec(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::hex2dec(s, mr); }

Value regexp(const Value &str, const Value &pat, std::pmr::memory_resource *mr) { return numkit::lang::regexpFind(str, pat, "", false, mr); }
Value regexpi(const Value &str, const Value &pat, std::pmr::memory_resource *mr) { return numkit::lang::regexpFind(str, pat, "", true, mr); }
Value regexprep(const Value &str, const Value &pat, const Value &rep, std::pmr::memory_resource *mr) { return numkit::lang::regexprep(str, pat, rep, false, false, mr); }
Value regexptranslate(const std::string &type, const Value &str, std::pmr::memory_resource *mr) {
    return numkit::lang::regexptranslate(type, str.isChar() || str.isString() ? str.toString() : "", mr);
}

// ── Registration Implementation ────────────────────────────────────────────

void register_strfun(Engine &engine) {
    engine.registerFunction("convertContainedStringsToChars",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("convertContainedStringsToChars: requires 1 argument");
            outs[0] = numkit::builtin::convertContainedStringsToChars(args[0], ctx.engine->resource());
        });

    engine.registerFunction("num2str",    &::numkit::builtin::detail::num2str_reg);
    engine.registerFunction("int2str",    &::numkit::builtin::detail::int2str_reg);
    engine.registerFunction("validatestring", &::numkit::builtin::detail::validatestring_reg);
    engine.registerFunction("str2num",    &::numkit::builtin::detail::str2num_reg);
    engine.registerFunction("str2double", &::numkit::builtin::detail::str2double_reg);
    engine.registerFunction("string",     &::numkit::builtin::detail::string_reg);
    engine.registerFunction("char",       &::numkit::builtin::detail::char_reg);
    engine.registerFunction("strcmp",     &::numkit::builtin::detail::strcmp_reg);
    engine.registerFunction("strcmpi",    &::numkit::builtin::detail::strcmpi_reg);
    engine.registerFunction("upper",      &::numkit::builtin::detail::upper_reg);
    engine.registerFunction("lower",      &::numkit::builtin::detail::lower_reg);
    engine.registerFunction("strtrim",    &::numkit::builtin::detail::strtrim_reg);
    engine.registerFunction("strsplit",   &::numkit::builtin::detail::strsplit_reg);
    engine.registerFunction("strcat",     &::numkit::builtin::detail::strcat_reg);
    engine.registerFunction("strlength",  &::numkit::builtin::detail::strlength_reg);
    engine.registerFunction("strrep",     &::numkit::builtin::detail::strrep_reg);
    engine.registerFunction("contains",   &::numkit::builtin::detail::contains_reg);
    engine.registerFunction("startsWith", &::numkit::builtin::detail::startsWith_reg);
    engine.registerFunction("endsWith",   &::numkit::builtin::detail::endsWith_reg);
    engine.registerFunction("extractafter",   &::numkit::builtin::detail::extractAfter_reg);
    engine.registerFunction("extractbefore",  &::numkit::builtin::detail::extractBefore_reg);
    engine.registerFunction("extractbetween", &::numkit::builtin::detail::extractBetween_reg);
    engine.registerFunction("erasebetween",   &::numkit::builtin::detail::eraseBetween_reg);
    engine.registerFunction("insertafter",    &::numkit::builtin::detail::insertAfter_reg);
    engine.registerFunction("insertbefore",   &::numkit::builtin::detail::insertBefore_reg);
    engine.registerFunction("replacebetween", &::numkit::builtin::detail::replaceBetween_reg);
    engine.registerFunction("strncmp",    &::numkit::builtin::detail::strncmp_reg);
    engine.registerFunction("strncmpi",   &::numkit::builtin::detail::strncmpi_reg);
    engine.registerFunction("strfind",    &::numkit::builtin::detail::strfind_reg);
    engine.registerFunction("blanks",     &::numkit::builtin::detail::blanks_reg);
    engine.registerFunction("newline",    &::numkit::builtin::detail::newline_reg);
    engine.registerFunction("strings",    &::numkit::builtin::detail::strings_reg);
    engine.registerFunction("compose",    &::numkit::builtin::detail::compose_reg);
    engine.registerFunction("strjust",    &::numkit::builtin::detail::strjust_reg);
    engine.registerFunction("extract",    &::numkit::builtin::detail::extract_reg);
    engine.registerFunction("split",      &::numkit::builtin::detail::split_reg);
    engine.registerFunction("join",       &::numkit::builtin::detail::join_reg);
    engine.registerFunction("deblank",    &::numkit::builtin::detail::deblank_reg);
    engine.registerFunction("mat2str",    &::numkit::builtin::detail::mat2str_reg);
    engine.registerFunction("strjoin",    &::numkit::builtin::detail::strjoin_reg);
    engine.registerFunction("strtok",     &::numkit::builtin::detail::strtok_reg);
    engine.registerFunction("append",     &::numkit::builtin::detail::append_reg);
    engine.registerFunction("count",      &::numkit::builtin::detail::count_reg);
    engine.registerFunction("erase",      &::numkit::builtin::detail::erase_reg);
    engine.registerFunction("replace",    &::numkit::builtin::detail::replace_reg);
    engine.registerFunction("reverse",    &::numkit::builtin::detail::reverse_reg);
    engine.registerFunction("splitlines", &::numkit::builtin::detail::splitlines_reg);
    engine.registerFunction("pad",        &::numkit::builtin::detail::pad_reg);
    engine.registerFunction("strip",      &::numkit::builtin::detail::strip_reg);
    engine.registerFunction("matches",    &::numkit::builtin::detail::matches_reg);
    engine.registerFunction("convertCharsToStrings",
                                          &::numkit::builtin::detail::convertCharsToStrings_reg);
    engine.registerFunction("convertStringsToChars",
                                          &::numkit::builtin::detail::convertStringsToChars_reg);
    engine.registerFunction("isstringscalar",
                                          &::numkit::builtin::detail::isstringscalar_reg);
    engine.registerFunction("isStringScalar",
                                          &::numkit::builtin::detail::isstringscalar_reg);
    engine.registerFunction("isstrprop",  &::numkit::builtin::detail::isstrprop_reg);
    engine.registerFunction("isletter",   &::numkit::builtin::detail::isletter_reg);
    engine.registerFunction("isspace",    &::numkit::builtin::detail::isspace_reg);
    engine.registerFunction("extractAfter",   &::numkit::builtin::detail::extractAfter_reg);
    engine.registerFunction("extractBefore",  &::numkit::builtin::detail::extractBefore_reg);
    engine.registerFunction("extractBetween", &::numkit::builtin::detail::extractBetween_reg);
    engine.registerFunction("insertAfter",    &::numkit::builtin::detail::insertAfter_reg);
    engine.registerFunction("insertBefore",   &::numkit::builtin::detail::insertBefore_reg);
    engine.registerFunction("eraseBetween",   &::numkit::builtin::detail::eraseBetween_reg);
    engine.registerFunction("replaceBetween", &::numkit::builtin::detail::replaceBetween_reg);
    engine.registerFunction("dec2bin",    &::numkit::builtin::detail::dec2bin_reg);
    engine.registerFunction("dec2hex",    &::numkit::builtin::detail::dec2hex_reg);
    engine.registerFunction("dec2base",   &::numkit::builtin::detail::dec2base_reg);
    engine.registerFunction("base2dec",   &::numkit::builtin::detail::base2dec_reg);
    engine.registerFunction("bin2dec",    &::numkit::builtin::detail::bin2dec_reg);
    engine.registerFunction("hex2dec",    &::numkit::builtin::detail::hex2dec_reg);
    engine.registerFunction("hex2num",    &::numkit::builtin::detail::hex2num_reg);
    engine.registerFunction("num2hex",    &::numkit::builtin::detail::num2hex_reg);
    engine.registerFunction("rat",        &::numkit::builtin::detail::rat_reg);
    engine.registerFunction("rats",       &::numkit::builtin::detail::rats_reg);
    engine.registerFunction("regexp",     &::numkit::builtin::detail::regexp_reg);
    engine.registerFunction("regexpi",    &::numkit::builtin::detail::regexpi_reg);
    engine.registerFunction("regexprep",  &::numkit::builtin::detail::regexprep_reg);
    engine.registerFunction("regexptranslate", &::numkit::builtin::detail::regexptranslate_reg);
}

} // namespace numkit::builtin
