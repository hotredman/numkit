// src/bundle/src/register/builtin/datatypes_reg.cpp
//
// Registration for numkit::builtin pure datatypes, conversions, and predicates.

#include <numkit/builtin/datatypes.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin::detail {
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
} // namespace numkit::builtin::detail

namespace numkit::bundle::builtin {

void register_datatypes(Engine &engine) {
    // Pure numeric and array type constructors & conversions
    engine.registerFunction("double",    &::numkit::builtin::detail::double_reg);
    engine.registerFunction("single",    &::numkit::builtin::detail::single_reg);
    engine.registerFunction("int8",      &::numkit::builtin::detail::int8_reg);
    engine.registerFunction("int16",     &::numkit::builtin::detail::int16_reg);
    engine.registerFunction("int32",     &::numkit::builtin::detail::int32_reg);
    engine.registerFunction("int64",     &::numkit::builtin::detail::int64_reg);
    engine.registerFunction("uint8",     &::numkit::builtin::detail::uint8_reg);
    engine.registerFunction("uint16",    &::numkit::builtin::detail::uint16_reg);
    engine.registerFunction("uint32",    &::numkit::builtin::detail::uint32_reg);
    engine.registerFunction("uint64",    &::numkit::builtin::detail::uint64_reg);
    engine.registerFunction("logical",   &::numkit::builtin::detail::logical_reg);
    engine.registerFunction("cast",      &::numkit::builtin::detail::cast_reg);
    engine.registerFunction("swapbytes", &::numkit::builtin::detail::swapbytes_reg);
    engine.registerFunction("typecast",  &::numkit::builtin::detail::typecast_reg);

    // Array type and shape predicates
    engine.registerFunction("isnumeric", &::numkit::builtin::detail::isnumeric_reg);
    engine.registerFunction("islogical", &::numkit::builtin::detail::islogical_reg);
    engine.registerFunction("ischar",    &::numkit::builtin::detail::ischar_reg);
    engine.registerFunction("isstring",  &::numkit::builtin::detail::isstring_reg);
    engine.registerFunction("iscell",    &::numkit::builtin::detail::iscell_reg);
    engine.registerFunction("isstruct",  &::numkit::builtin::detail::isstruct_reg);
    engine.registerFunction("isempty",   &::numkit::builtin::detail::isempty_reg);
    engine.registerFunction("isscalar",  &::numkit::builtin::detail::isscalar_reg);
    engine.registerFunction("isreal",    &::numkit::builtin::detail::isreal_reg);
    engine.registerFunction("isinteger", &::numkit::builtin::detail::isinteger_reg);
    engine.registerFunction("isfloat",   &::numkit::builtin::detail::isfloat_reg);
    engine.registerFunction("issingle",  &::numkit::builtin::detail::issingle_reg);
    engine.registerFunction("issparse",  &::numkit::builtin::detail::issparse_reg);
    engine.registerFunction("isnan",     &::numkit::builtin::detail::isnan_reg);
    engine.registerFunction("isinf",     &::numkit::builtin::detail::isinf_reg);
    engine.registerFunction("isfinite",  &::numkit::builtin::detail::isfinite_reg);
    engine.registerFunction("ismissing", &::numkit::builtin::detail::ismissing_reg);
    engine.registerFunction("anymissing",&::numkit::builtin::detail::anymissing_reg);
    engine.registerFunction("standardizeMissing", &::numkit::builtin::detail::standardizeMissing_reg);
    engine.registerFunction("isvector",   &::numkit::builtin::detail::isvector_reg);
    engine.registerFunction("isrow",      &::numkit::builtin::detail::isrow_reg);
    engine.registerFunction("iscolumn",   &::numkit::builtin::detail::iscolumn_reg);
    engine.registerFunction("ismatrix",   &::numkit::builtin::detail::ismatrix_reg);
    engine.registerFunction("issorted",   &::numkit::builtin::detail::issorted_reg);
    engine.registerFunction("issortedrows",&::numkit::builtin::detail::issortedrows_reg);
    engine.registerFunction("isuniform",  &::numkit::builtin::detail::isuniform_reg);

    // Numeric limits
    engine.registerFunction("flintmax",   &::numkit::builtin::detail::flintmax_reg);
    engine.registerFunction("intmax",     &::numkit::builtin::detail::intmax_reg);
    engine.registerFunction("intmin",     &::numkit::builtin::detail::intmin_reg);
    engine.registerFunction("realmax",    &::numkit::builtin::detail::realmax_reg);
    engine.registerFunction("realmin",    &::numkit::builtin::detail::realmin_reg);
    engine.registerFunction("allfinite",  &::numkit::builtin::detail::allfinite_reg);
    engine.registerFunction("anynan",     &::numkit::builtin::detail::anynan_reg);
    engine.registerFunction("isequal",   &::numkit::builtin::detail::isequal_reg);
    engine.registerFunction("isequaln",  &::numkit::builtin::detail::isequaln_reg);
}

} // namespace numkit::bundle::builtin
