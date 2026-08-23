// src/bundle/src/register/builtin/strfun_reg.cpp

#include <numkit/builtin/strfun.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <stdexcept>

namespace numkit::builtin::detail {

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

} // namespace numkit::builtin::detail

namespace numkit::bundle::builtin {

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

} // namespace numkit::bundle::builtin
