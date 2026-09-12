// src/bundle/src/register/builtin/strfun_reg.cpp

#include <nk/builtin/strfun.hpp>
#include <nk/core/engine.hpp>
#include <nk/value/value.hpp>
#include <nk/value/span.hpp>

#include <stdexcept>

namespace nk::builtin::detail {

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

} // namespace nk::builtin::detail

namespace nk::bundle::builtin {

void register_strfun(Engine &engine) {
    engine.registerFunction("convertContainedStringsToChars",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("convertContainedStringsToChars: requires 1 argument");
            outs[0] = nk::builtin::convertContainedStringsToChars(args[0], ctx.engine->resource());
        });

    engine.registerFunction("num2str",    &::nk::builtin::detail::num2str_reg);
    engine.registerFunction("int2str",    &::nk::builtin::detail::int2str_reg);
    engine.registerFunction("validatestring", &::nk::builtin::detail::validatestring_reg);
    engine.registerFunction("str2num",    &::nk::builtin::detail::str2num_reg);
    engine.registerFunction("str2double", &::nk::builtin::detail::str2double_reg);
    engine.registerFunction("string",     &::nk::builtin::detail::string_reg);
    engine.registerFunction("char",       &::nk::builtin::detail::char_reg);
    engine.registerFunction("strcmp",     &::nk::builtin::detail::strcmp_reg);
    engine.registerFunction("strcmpi",    &::nk::builtin::detail::strcmpi_reg);
    engine.registerFunction("upper",      &::nk::builtin::detail::upper_reg);
    engine.registerFunction("lower",      &::nk::builtin::detail::lower_reg);
    engine.registerFunction("strtrim",    &::nk::builtin::detail::strtrim_reg);
    engine.registerFunction("strsplit",   &::nk::builtin::detail::strsplit_reg);
    engine.registerFunction("strcat",     &::nk::builtin::detail::strcat_reg);
    engine.registerFunction("strlength",  &::nk::builtin::detail::strlength_reg);
    engine.registerFunction("strrep",     &::nk::builtin::detail::strrep_reg);
    engine.registerFunction("contains",   &::nk::builtin::detail::contains_reg);
    engine.registerFunction("startsWith", &::nk::builtin::detail::startsWith_reg);
    engine.registerFunction("endsWith",   &::nk::builtin::detail::endsWith_reg);
    engine.registerFunction("extractafter",   &::nk::builtin::detail::extractAfter_reg);
    engine.registerFunction("extractbefore",  &::nk::builtin::detail::extractBefore_reg);
    engine.registerFunction("extractbetween", &::nk::builtin::detail::extractBetween_reg);
    engine.registerFunction("erasebetween",   &::nk::builtin::detail::eraseBetween_reg);
    engine.registerFunction("insertafter",    &::nk::builtin::detail::insertAfter_reg);
    engine.registerFunction("insertbefore",   &::nk::builtin::detail::insertBefore_reg);
    engine.registerFunction("replacebetween", &::nk::builtin::detail::replaceBetween_reg);
    engine.registerFunction("strncmp",    &::nk::builtin::detail::strncmp_reg);
    engine.registerFunction("strncmpi",   &::nk::builtin::detail::strncmpi_reg);
    engine.registerFunction("strfind",    &::nk::builtin::detail::strfind_reg);
    engine.registerFunction("blanks",     &::nk::builtin::detail::blanks_reg);
    engine.registerFunction("newline",    &::nk::builtin::detail::newline_reg);
    engine.registerFunction("strings",    &::nk::builtin::detail::strings_reg);
    engine.registerFunction("compose",    &::nk::builtin::detail::compose_reg);
    engine.registerFunction("strjust",    &::nk::builtin::detail::strjust_reg);
    engine.registerFunction("extract",    &::nk::builtin::detail::extract_reg);
    engine.registerFunction("split",      &::nk::builtin::detail::split_reg);
    engine.registerFunction("join",       &::nk::builtin::detail::join_reg);
    engine.registerFunction("deblank",    &::nk::builtin::detail::deblank_reg);
    engine.registerFunction("mat2str",    &::nk::builtin::detail::mat2str_reg);
    engine.registerFunction("strjoin",    &::nk::builtin::detail::strjoin_reg);
    engine.registerFunction("strtok",     &::nk::builtin::detail::strtok_reg);
    engine.registerFunction("append",     &::nk::builtin::detail::append_reg);
    engine.registerFunction("count",      &::nk::builtin::detail::count_reg);
    engine.registerFunction("erase",      &::nk::builtin::detail::erase_reg);
    engine.registerFunction("replace",    &::nk::builtin::detail::replace_reg);
    engine.registerFunction("reverse",    &::nk::builtin::detail::reverse_reg);
    engine.registerFunction("splitlines", &::nk::builtin::detail::splitlines_reg);
    engine.registerFunction("pad",        &::nk::builtin::detail::pad_reg);
    engine.registerFunction("strip",      &::nk::builtin::detail::strip_reg);
    engine.registerFunction("matches",    &::nk::builtin::detail::matches_reg);
    engine.registerFunction("convertCharsToStrings",
                                          &::nk::builtin::detail::convertCharsToStrings_reg);
    engine.registerFunction("convertStringsToChars",
                                          &::nk::builtin::detail::convertStringsToChars_reg);
    engine.registerFunction("isstringscalar",
                                          &::nk::builtin::detail::isstringscalar_reg);
    engine.registerFunction("isStringScalar",
                                          &::nk::builtin::detail::isstringscalar_reg);
    engine.registerFunction("isstrprop",  &::nk::builtin::detail::isstrprop_reg);
    engine.registerFunction("isletter",   &::nk::builtin::detail::isletter_reg);
    engine.registerFunction("isspace",    &::nk::builtin::detail::isspace_reg);
    engine.registerFunction("extractAfter",   &::nk::builtin::detail::extractAfter_reg);
    engine.registerFunction("extractBefore",  &::nk::builtin::detail::extractBefore_reg);
    engine.registerFunction("extractBetween", &::nk::builtin::detail::extractBetween_reg);
    engine.registerFunction("insertAfter",    &::nk::builtin::detail::insertAfter_reg);
    engine.registerFunction("insertBefore",   &::nk::builtin::detail::insertBefore_reg);
    engine.registerFunction("eraseBetween",   &::nk::builtin::detail::eraseBetween_reg);
    engine.registerFunction("replaceBetween", &::nk::builtin::detail::replaceBetween_reg);
    engine.registerFunction("dec2bin",    &::nk::builtin::detail::dec2bin_reg);
    engine.registerFunction("dec2hex",    &::nk::builtin::detail::dec2hex_reg);
    engine.registerFunction("dec2base",   &::nk::builtin::detail::dec2base_reg);
    engine.registerFunction("base2dec",   &::nk::builtin::detail::base2dec_reg);
    engine.registerFunction("bin2dec",    &::nk::builtin::detail::bin2dec_reg);
    engine.registerFunction("hex2dec",    &::nk::builtin::detail::hex2dec_reg);
    engine.registerFunction("hex2num",    &::nk::builtin::detail::hex2num_reg);
    engine.registerFunction("num2hex",    &::nk::builtin::detail::num2hex_reg);
    engine.registerFunction("rat",        &::nk::builtin::detail::rat_reg);
    engine.registerFunction("rats",       &::nk::builtin::detail::rats_reg);
    engine.registerFunction("regexp",     &::nk::builtin::detail::regexp_reg);
    engine.registerFunction("regexpi",    &::nk::builtin::detail::regexpi_reg);
    engine.registerFunction("regexprep",  &::nk::builtin::detail::regexprep_reg);
    engine.registerFunction("regexptranslate", &::nk::builtin::detail::regexptranslate_reg);
}

} // namespace nk::bundle::builtin
