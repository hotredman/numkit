// src/bundle/src/register/builtin/elfun_reg.cpp

#include <numkit/builtin/elfun.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin::detail {
void abs_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acos_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acosd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acosh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acot_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acotd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acoth_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acsc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acscd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acsch_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void angle_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void asec_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void asecd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void asech_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void asin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void asind_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void asinh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void atan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void atan2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void atan2d_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void atand_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void atanh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cart2pol_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cart2sph_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ceil_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void complex_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void conj_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cos_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cosd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cosh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cospi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cot_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cotd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void coth_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void csc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cscd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void csch_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void exp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void expm1_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fix_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void floor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void hypot_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void imag_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void log_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void log10_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void log1p_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void log2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mod_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pol2cart_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pow_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void real_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rem_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void round_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sec_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void secd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sech_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sign_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sind_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sinh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sinpi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sph2cart_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sqrt_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void subplus_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void tan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void tand_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void tanh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
} // namespace numkit::builtin::detail

namespace numkit::bundle::builtin {

void register_elfun(Engine &engine) {
    engine.registerFunction("sqrt",     &::numkit::builtin::detail::sqrt_reg);
    engine.registerFunction("abs",      &::numkit::builtin::detail::abs_reg);
    engine.registerFunction("sin",      &::numkit::builtin::detail::sin_reg);
    engine.registerFunction("cos",      &::numkit::builtin::detail::cos_reg);
    engine.registerFunction("tan",      &::numkit::builtin::detail::tan_reg);
    engine.registerFunction("asin",     &::numkit::builtin::detail::asin_reg);
    engine.registerFunction("acos",     &::numkit::builtin::detail::acos_reg);
    engine.registerFunction("atan",     &::numkit::builtin::detail::atan_reg);
    engine.registerFunction("atan2",    &::numkit::builtin::detail::atan2_reg);
    engine.registerFunction("sinh",     &::numkit::builtin::detail::sinh_reg);
    engine.registerFunction("cosh",     &::numkit::builtin::detail::cosh_reg);
    engine.registerFunction("tanh",     &::numkit::builtin::detail::tanh_reg);
    engine.registerFunction("asinh",    &::numkit::builtin::detail::asinh_reg);
    engine.registerFunction("acosh",    &::numkit::builtin::detail::acosh_reg);
    engine.registerFunction("atanh",    &::numkit::builtin::detail::atanh_reg);
    engine.registerFunction("sind",     &::numkit::builtin::detail::sind_reg);
    engine.registerFunction("cosd",     &::numkit::builtin::detail::cosd_reg);
    engine.registerFunction("tand",     &::numkit::builtin::detail::tand_reg);
    engine.registerFunction("asind",    &::numkit::builtin::detail::asind_reg);
    engine.registerFunction("acosd",    &::numkit::builtin::detail::acosd_reg);
    engine.registerFunction("atand",    &::numkit::builtin::detail::atand_reg);
    engine.registerFunction("atan2d",   &::numkit::builtin::detail::atan2d_reg);
    engine.registerFunction("sinpi",    &::numkit::builtin::detail::sinpi_reg);
    engine.registerFunction("cospi",    &::numkit::builtin::detail::cospi_reg);
    engine.registerFunction("sec",      &::numkit::builtin::detail::sec_reg);
    engine.registerFunction("csc",      &::numkit::builtin::detail::csc_reg);
    engine.registerFunction("cot",      &::numkit::builtin::detail::cot_reg);
    engine.registerFunction("sech",     &::numkit::builtin::detail::sech_reg);
    engine.registerFunction("csch",     &::numkit::builtin::detail::csch_reg);
    engine.registerFunction("coth",     &::numkit::builtin::detail::coth_reg);
    engine.registerFunction("secd",     &::numkit::builtin::detail::secd_reg);
    engine.registerFunction("cscd",     &::numkit::builtin::detail::cscd_reg);
    engine.registerFunction("cotd",     &::numkit::builtin::detail::cotd_reg);
    engine.registerFunction("asec",     &::numkit::builtin::detail::asec_reg);
    engine.registerFunction("acsc",     &::numkit::builtin::detail::acsc_reg);
    engine.registerFunction("acot",     &::numkit::builtin::detail::acot_reg);
    engine.registerFunction("asech",    &::numkit::builtin::detail::asech_reg);
    engine.registerFunction("acsch",    &::numkit::builtin::detail::acsch_reg);
    engine.registerFunction("acoth",    &::numkit::builtin::detail::acoth_reg);
    engine.registerFunction("asecd",    &::numkit::builtin::detail::asecd_reg);
    engine.registerFunction("acscd",    &::numkit::builtin::detail::acscd_reg);
    engine.registerFunction("acotd",    &::numkit::builtin::detail::acotd_reg);
    engine.registerFunction("cart2pol", &::numkit::builtin::detail::cart2pol_reg);
    engine.registerFunction("pol2cart", &::numkit::builtin::detail::pol2cart_reg);
    engine.registerFunction("cart2sph", &::numkit::builtin::detail::cart2sph_reg);
    engine.registerFunction("sph2cart", &::numkit::builtin::detail::sph2cart_reg);
    engine.registerFunction("exp",      &::numkit::builtin::detail::exp_reg);
    engine.registerFunction("log",      &::numkit::builtin::detail::log_reg);
    engine.registerFunction("log2",     &::numkit::builtin::detail::log2_reg);
    engine.registerFunction("log10",    &::numkit::builtin::detail::log10_reg);
    engine.registerFunction("floor",    &::numkit::builtin::detail::floor_reg);
    engine.registerFunction("ceil",     &::numkit::builtin::detail::ceil_reg);
    engine.registerFunction("round",    &::numkit::builtin::detail::round_reg);
    engine.registerFunction("fix",      &::numkit::builtin::detail::fix_reg);
    engine.registerFunction("mod",      &::numkit::builtin::detail::mod_reg);
    engine.registerFunction("rem",      &::numkit::builtin::detail::rem_reg);
    engine.registerFunction("sign",     &::numkit::builtin::detail::sign_reg);
    engine.registerFunction("subplus",  &::numkit::builtin::detail::subplus_reg);
    engine.registerFunction("real",     &::numkit::builtin::detail::real_reg);
    engine.registerFunction("imag",     &::numkit::builtin::detail::imag_reg);
    engine.registerFunction("conj",     &::numkit::builtin::detail::conj_reg);
    engine.registerFunction("angle",    &::numkit::builtin::detail::angle_reg);
    engine.registerFunction("complex",  &::numkit::builtin::detail::complex_reg);
    engine.registerFunction("hypot",    &::numkit::builtin::detail::hypot_reg);
}

} // namespace numkit::bundle::builtin
