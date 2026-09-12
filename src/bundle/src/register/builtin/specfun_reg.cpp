// src/bundle/src/register/builtin/specfun_reg.cpp

#include <nk/builtin/specfun.hpp>
#include <nk/core/engine.hpp>
#include <nk/value/value.hpp>
#include <nk/value/span.hpp>

namespace nk::builtin::detail {

void airy_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void besselh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void besseli_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void besselj_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void besselk_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bessely_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void beta_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void betainc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void betaincinv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void betaln_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ellipj_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ellipke_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erfc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erfcinv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erfcx_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erfinv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void expint_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void factor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void factorial_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gamma_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gammainc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gammaincinv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gammaln_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gcd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isprime_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void lcm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void legendre_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nchoosek_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void perms_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void primes_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void psi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

} // namespace nk::builtin::detail

namespace nk::bundle::builtin {

void register_specfun(Engine &engine) {
    engine.registerFunction("airy",        &::nk::builtin::detail::airy_reg);
    engine.registerFunction("besselh",     &::nk::builtin::detail::besselh_reg);
    engine.registerFunction("besseli",     &::nk::builtin::detail::besseli_reg);
    engine.registerFunction("besselj",     &::nk::builtin::detail::besselj_reg);
    engine.registerFunction("besselk",     &::nk::builtin::detail::besselk_reg);
    engine.registerFunction("bessely",     &::nk::builtin::detail::bessely_reg);
    engine.registerFunction("beta",        &::nk::builtin::detail::beta_reg);
    engine.registerFunction("betainc",     &::nk::builtin::detail::betainc_reg);
    engine.registerFunction("betaincinv",  &::nk::builtin::detail::betaincinv_reg);
    engine.registerFunction("betaln",      &::nk::builtin::detail::betaln_reg);
    engine.registerFunction("ellipj",      &::nk::builtin::detail::ellipj_reg);
    engine.registerFunction("ellipke",     &::nk::builtin::detail::ellipke_reg);
    engine.registerFunction("erf",         &::nk::builtin::detail::erf_reg);
    engine.registerFunction("erfc",        &::nk::builtin::detail::erfc_reg);
    engine.registerFunction("erfcinv",     &::nk::builtin::detail::erfcinv_reg);
    engine.registerFunction("erfcx",       &::nk::builtin::detail::erfcx_reg);
    engine.registerFunction("erfinv",      &::nk::builtin::detail::erfinv_reg);
    engine.registerFunction("expint",      &::nk::builtin::detail::expint_reg);
    engine.registerFunction("factor",      &::nk::builtin::detail::factor_reg);
    engine.registerFunction("factorial",   &::nk::builtin::detail::factorial_reg);
    engine.registerFunction("gamma",       &::nk::builtin::detail::gamma_reg);
    engine.registerFunction("gammainc",    &::nk::builtin::detail::gammainc_reg);
    engine.registerFunction("gammaincinv", &::nk::builtin::detail::gammaincinv_reg);
    engine.registerFunction("gammaln",     &::nk::builtin::detail::gammaln_reg);
    engine.registerFunction("gcd",         &::nk::builtin::detail::gcd_reg);
    engine.registerFunction("isprime",     &::nk::builtin::detail::isprime_reg);
    engine.registerFunction("lcm",         &::nk::builtin::detail::lcm_reg);
    engine.registerFunction("legendre",    &::nk::builtin::detail::legendre_reg);
    engine.registerFunction("nchoosek",    &::nk::builtin::detail::nchoosek_reg);
    engine.registerFunction("perms",       &::nk::builtin::detail::perms_reg);
    engine.registerFunction("primes",      &::nk::builtin::detail::primes_reg);
    engine.registerFunction("psi",         &::nk::builtin::detail::psi_reg);
}

} // namespace nk::bundle::builtin
