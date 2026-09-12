// src/bundle/src/register/builtin/datatypes_reg.cpp
//
// Registration for nk::builtin pure datatypes, conversions, and predicates.

#include <nk/builtin/datatypes.hpp>
#include <nk/core/engine.hpp>
#include <nk/value/value.hpp>
#include <nk/value/span.hpp>

namespace nk::builtin::detail {
void allfinite_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void anymissing_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void anynan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cast_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void double_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void flintmax_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int16_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int32_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int64_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int8_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void intmax_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void intmin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void iscell_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ischar_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void iscolumn_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isempty_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isequal_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isequaln_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isfinite_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isfloat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isinf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isinteger_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void islogical_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismatrix_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismissing_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isnan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isnumeric_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isreal_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isrow_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isscalar_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issingle_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issorted_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issortedrows_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issparse_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isstring_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isstruct_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isuniform_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isvector_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void logical_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void realmax_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void realmin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void single_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void standardizeMissing_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void swapbytes_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void typecast_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uint16_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uint32_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uint64_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uint8_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
} // namespace nk::builtin::detail

namespace nk::bundle::builtin {

void register_datatypes(Engine &engine) {
    // Pure numeric and array type constructors & conversions
    engine.registerFunction("double",    &::nk::builtin::detail::double_reg);
    engine.registerFunction("single",    &::nk::builtin::detail::single_reg);
    engine.registerFunction("int8",      &::nk::builtin::detail::int8_reg);
    engine.registerFunction("int16",     &::nk::builtin::detail::int16_reg);
    engine.registerFunction("int32",     &::nk::builtin::detail::int32_reg);
    engine.registerFunction("int64",     &::nk::builtin::detail::int64_reg);
    engine.registerFunction("uint8",     &::nk::builtin::detail::uint8_reg);
    engine.registerFunction("uint16",    &::nk::builtin::detail::uint16_reg);
    engine.registerFunction("uint32",    &::nk::builtin::detail::uint32_reg);
    engine.registerFunction("uint64",    &::nk::builtin::detail::uint64_reg);
    engine.registerFunction("logical",   &::nk::builtin::detail::logical_reg);
    engine.registerFunction("cast",      &::nk::builtin::detail::cast_reg);
    engine.registerFunction("swapbytes", &::nk::builtin::detail::swapbytes_reg);
    engine.registerFunction("typecast",  &::nk::builtin::detail::typecast_reg);

    // Array type and shape predicates
    engine.registerFunction("isnumeric", &::nk::builtin::detail::isnumeric_reg);
    engine.registerFunction("islogical", &::nk::builtin::detail::islogical_reg);
    engine.registerFunction("ischar",    &::nk::builtin::detail::ischar_reg);
    engine.registerFunction("isstring",  &::nk::builtin::detail::isstring_reg);
    engine.registerFunction("iscell",    &::nk::builtin::detail::iscell_reg);
    engine.registerFunction("isstruct",  &::nk::builtin::detail::isstruct_reg);
    engine.registerFunction("isempty",   &::nk::builtin::detail::isempty_reg);
    engine.registerFunction("isscalar",  &::nk::builtin::detail::isscalar_reg);
    engine.registerFunction("isreal",    &::nk::builtin::detail::isreal_reg);
    engine.registerFunction("isinteger", &::nk::builtin::detail::isinteger_reg);
    engine.registerFunction("isfloat",   &::nk::builtin::detail::isfloat_reg);
    engine.registerFunction("issingle",  &::nk::builtin::detail::issingle_reg);
    engine.registerFunction("issparse",  &::nk::builtin::detail::issparse_reg);
    engine.registerFunction("isnan",     &::nk::builtin::detail::isnan_reg);
    engine.registerFunction("isinf",     &::nk::builtin::detail::isinf_reg);
    engine.registerFunction("isfinite",  &::nk::builtin::detail::isfinite_reg);
    engine.registerFunction("ismissing", &::nk::builtin::detail::ismissing_reg);
    engine.registerFunction("anymissing",&::nk::builtin::detail::anymissing_reg);
    engine.registerFunction("standardizeMissing", &::nk::builtin::detail::standardizeMissing_reg);
    engine.registerFunction("isvector",   &::nk::builtin::detail::isvector_reg);
    engine.registerFunction("isrow",      &::nk::builtin::detail::isrow_reg);
    engine.registerFunction("iscolumn",   &::nk::builtin::detail::iscolumn_reg);
    engine.registerFunction("ismatrix",   &::nk::builtin::detail::ismatrix_reg);
    engine.registerFunction("issorted",   &::nk::builtin::detail::issorted_reg);
    engine.registerFunction("issortedrows",&::nk::builtin::detail::issortedrows_reg);
    engine.registerFunction("isuniform",  &::nk::builtin::detail::isuniform_reg);

    // Numeric limits
    engine.registerFunction("flintmax",   &::nk::builtin::detail::flintmax_reg);
    engine.registerFunction("intmax",     &::nk::builtin::detail::intmax_reg);
    engine.registerFunction("intmin",     &::nk::builtin::detail::intmin_reg);
    engine.registerFunction("realmax",    &::nk::builtin::detail::realmax_reg);
    engine.registerFunction("realmin",    &::nk::builtin::detail::realmin_reg);
    engine.registerFunction("allfinite",  &::nk::builtin::detail::allfinite_reg);
    engine.registerFunction("anynan",     &::nk::builtin::detail::anynan_reg);
    engine.registerFunction("isequal",   &::nk::builtin::detail::isequal_reg);
    engine.registerFunction("isequaln",  &::nk::builtin::detail::isequaln_reg);
}

} // namespace nk::bundle::builtin
