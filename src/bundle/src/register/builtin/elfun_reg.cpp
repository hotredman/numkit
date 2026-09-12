// src/bundle/src/register/builtin/elfun_reg.cpp

#include <nk/builtin/elfun.hpp>
#include <nk/core/engine.hpp>
#include <nk/core/types.hpp>
#include <nk/value/value.hpp>
#include <nk/value/span.hpp>

namespace nk::builtin::detail {
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
} // namespace nk::builtin::detail

namespace nk::bundle::builtin {

void register_elfun(Engine &engine) {
    engine.registerFunction("sqrt",     &::nk::builtin::detail::sqrt_reg);
    engine.registerFunction("abs",      &::nk::builtin::detail::abs_reg);
    engine.registerFunction("sin",      &::nk::builtin::detail::sin_reg);
    engine.registerFunction("cos",      &::nk::builtin::detail::cos_reg);
    engine.registerFunction("tan",      &::nk::builtin::detail::tan_reg);
    engine.registerFunction("asin",     &::nk::builtin::detail::asin_reg);
    engine.registerFunction("acos",     &::nk::builtin::detail::acos_reg);
    engine.registerFunction("atan",     &::nk::builtin::detail::atan_reg);
    engine.registerFunction("atan2",    &::nk::builtin::detail::atan2_reg);
    engine.registerFunction("sinh",     &::nk::builtin::detail::sinh_reg);
    engine.registerFunction("cosh",     &::nk::builtin::detail::cosh_reg);
    engine.registerFunction("tanh",     &::nk::builtin::detail::tanh_reg);
    engine.registerFunction("asinh",    &::nk::builtin::detail::asinh_reg);
    engine.registerFunction("acosh",    &::nk::builtin::detail::acosh_reg);
    engine.registerFunction("atanh",    &::nk::builtin::detail::atanh_reg);
    engine.registerFunction("sind",     &::nk::builtin::detail::sind_reg);
    engine.registerFunction("cosd",     &::nk::builtin::detail::cosd_reg);
    engine.registerFunction("tand",     &::nk::builtin::detail::tand_reg);
    engine.registerFunction("asind",    &::nk::builtin::detail::asind_reg);
    engine.registerFunction("acosd",    &::nk::builtin::detail::acosd_reg);
    engine.registerFunction("atand",    &::nk::builtin::detail::atand_reg);
    engine.registerFunction("atan2d",   &::nk::builtin::detail::atan2d_reg);
    engine.registerFunction("sinpi",    &::nk::builtin::detail::sinpi_reg);
    engine.registerFunction("cospi",    &::nk::builtin::detail::cospi_reg);
    engine.registerFunction("sec",      &::nk::builtin::detail::sec_reg);
    engine.registerFunction("csc",      &::nk::builtin::detail::csc_reg);
    engine.registerFunction("cot",      &::nk::builtin::detail::cot_reg);
    engine.registerFunction("sech",     &::nk::builtin::detail::sech_reg);
    engine.registerFunction("csch",     &::nk::builtin::detail::csch_reg);
    engine.registerFunction("coth",     &::nk::builtin::detail::coth_reg);
    engine.registerFunction("secd",     &::nk::builtin::detail::secd_reg);
    engine.registerFunction("cscd",     &::nk::builtin::detail::cscd_reg);
    engine.registerFunction("cotd",     &::nk::builtin::detail::cotd_reg);
    engine.registerFunction("asec",     &::nk::builtin::detail::asec_reg);
    engine.registerFunction("acsc",     &::nk::builtin::detail::acsc_reg);
    engine.registerFunction("acot",     &::nk::builtin::detail::acot_reg);
    engine.registerFunction("asech",    &::nk::builtin::detail::asech_reg);
    engine.registerFunction("acsch",    &::nk::builtin::detail::acsch_reg);
    engine.registerFunction("acoth",    &::nk::builtin::detail::acoth_reg);
    engine.registerFunction("asecd",    &::nk::builtin::detail::asecd_reg);
    engine.registerFunction("acscd",    &::nk::builtin::detail::acscd_reg);
    engine.registerFunction("acotd",    &::nk::builtin::detail::acotd_reg);
    engine.registerFunction("cart2pol", &::nk::builtin::detail::cart2pol_reg);
    engine.registerFunction("pol2cart", &::nk::builtin::detail::pol2cart_reg);
    engine.registerFunction("cart2sph", &::nk::builtin::detail::cart2sph_reg);
    engine.registerFunction("sph2cart", &::nk::builtin::detail::sph2cart_reg);
    engine.registerFunction("exp",      &::nk::builtin::detail::exp_reg);
    engine.registerFunction("log",      &::nk::builtin::detail::log_reg);
    engine.registerFunction("log2",     &::nk::builtin::detail::log2_reg);
    engine.registerFunction("log10",    &::nk::builtin::detail::log10_reg);
    engine.registerFunction("floor",    &::nk::builtin::detail::floor_reg);
    engine.registerFunction("ceil",     &::nk::builtin::detail::ceil_reg);
    engine.registerFunction("round",    &::nk::builtin::detail::round_reg);
    engine.registerFunction("fix",      &::nk::builtin::detail::fix_reg);
    engine.registerFunction("mod",      &::nk::builtin::detail::mod_reg);
    engine.registerFunction("rem",      &::nk::builtin::detail::rem_reg);
    engine.registerFunction("sign",     &::nk::builtin::detail::sign_reg);
    engine.registerFunction("subplus",  &::nk::builtin::detail::subplus_reg);
    engine.registerFunction("real",     &::nk::builtin::detail::real_reg);
    engine.registerFunction("imag",     &::nk::builtin::detail::imag_reg);
    engine.registerFunction("conj",     &::nk::builtin::detail::conj_reg);
    engine.registerFunction("angle",    &::nk::builtin::detail::angle_reg);
    engine.registerFunction("complex",  &::nk::builtin::detail::complex_reg);
    engine.registerFunction("hypot",    &::nk::builtin::detail::hypot_reg);
}

} // namespace nk::bundle::builtin
