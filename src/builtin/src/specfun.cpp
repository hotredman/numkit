// src/builtin/src/specfun.cpp
//
// Special mathematical functions implementations.
#include <numkit/builtin/specfun.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/value.hpp>
#include <numkit/math/special/special.hpp>
#include <numkit/math/discrete/discrete.hpp>
#include <numkit/math/permutations.hpp>
#include <numkit/lang/bitwise/int_math.hpp>

namespace numkit::builtin {

Value gamma(const Value &x, std::pmr::memory_resource *mr) { return numkit::math::gammaFn(x, mr); }
Value gammaln(const Value &x, std::pmr::memory_resource *mr) { return numkit::math::gammaln(x, mr); }
Value gammainc(const Value &x, const Value &a, std::pmr::memory_resource *mr) { return numkit::math::gammainc(x, a, mr); }
Value gammaincinv(const Value &y, const Value &a, std::pmr::memory_resource *mr) { return numkit::math::gammaincinv(y, a, mr); }
Value psi(const Value &x, std::pmr::memory_resource *mr) { return numkit::math::psi(x, mr); }

Value beta(const Value &z, const Value &w, std::pmr::memory_resource *mr) { return numkit::math::beta(z, w, mr); }
Value betaln(const Value &z, const Value &w, std::pmr::memory_resource *mr) { return numkit::math::betaln(z, w, mr); }
Value betainc(const Value &x, const Value &z, const Value &w, std::pmr::memory_resource *mr) { return numkit::math::betainc(x, z, w, mr); }
Value betaincinv(const Value &y, const Value &z, const Value &w, std::pmr::memory_resource *mr) { return numkit::math::betaincinv(y, z, w, mr); }

Value erf(const Value &x, std::pmr::memory_resource *mr) { return numkit::math::erf(x, mr); }
Value erfc(const Value &x, std::pmr::memory_resource *mr) { return numkit::math::erfc(x, mr); }
Value erfcx(const Value &x, std::pmr::memory_resource *mr) { return numkit::math::erfcx(x, mr); }
Value erfinv(const Value &x, std::pmr::memory_resource *mr) { return numkit::math::erfinv(x, mr); }
Value erfcinv(const Value &x, std::pmr::memory_resource *mr) { return numkit::math::erfcinv(x, mr); }

Value besselj(const Value &nu, const Value &z, std::pmr::memory_resource *mr) { return numkit::math::besselj(nu, z, mr); }
Value bessely(const Value &nu, const Value &z, std::pmr::memory_resource *mr) { return numkit::math::bessely(nu, z, mr); }
Value besseli(const Value &nu, const Value &z, std::pmr::memory_resource *mr) { return numkit::math::besseli(nu, z, mr); }
Value besselk(const Value &nu, const Value &z, std::pmr::memory_resource *mr) { return numkit::math::besselk(nu, z, mr); }
Value besselh(const Value &nu, int k, const Value &z, std::pmr::memory_resource *mr) { return numkit::math::besselh(nu, k, z, mr); }

Value airy(const Value &z, std::pmr::memory_resource *mr) { return numkit::math::airy(0, z, mr); }
Value expint(const Value &x, std::pmr::memory_resource *mr) { return numkit::math::expint(x, mr); }
Value ellipke(const Value &m, std::pmr::memory_resource *mr) { return numkit::math::ellipke(m, mr).K; }
Value legendre(int n, const Value &x, std::pmr::memory_resource *mr) { return numkit::math::legendre(n, x, mr); }

Value factorial(const Value &n, std::pmr::memory_resource *mr) { return numkit::math::factorial(n, mr); }
Value nchoosek(const Value &v, int k, std::pmr::memory_resource *mr) {
    if (v.isScalar()) {
        return numkit::math::nchoosek(v.toScalar(), static_cast<double>(k), mr);
    }
    return numkit::math::nchoosekCombinations(v, static_cast<double>(k), mr);
}
Value perms(const Value &v, std::pmr::memory_resource *mr) { return numkit::math::perms(v, mr); }
Value gcd(const Value &a, const Value &b, std::pmr::memory_resource *mr) { return numkit::lang::gcd(a, b, mr); }
Value lcm(const Value &a, const Value &b, std::pmr::memory_resource *mr) { return numkit::lang::lcm(a, b, mr); }

namespace detail {
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
} // namespace detail

void register_specfun(Engine &engine) {
    engine.registerFunction("airy",        &::numkit::builtin::detail::airy_reg);
    engine.registerFunction("besselh",     &::numkit::builtin::detail::besselh_reg);
    engine.registerFunction("besseli",     &::numkit::builtin::detail::besseli_reg);
    engine.registerFunction("besselj",     &::numkit::builtin::detail::besselj_reg);
    engine.registerFunction("besselk",     &::numkit::builtin::detail::besselk_reg);
    engine.registerFunction("bessely",     &::numkit::builtin::detail::bessely_reg);
    engine.registerFunction("beta",        &::numkit::builtin::detail::beta_reg);
    engine.registerFunction("betainc",     &::numkit::builtin::detail::betainc_reg);
    engine.registerFunction("betaincinv",  &::numkit::builtin::detail::betaincinv_reg);
    engine.registerFunction("betaln",      &::numkit::builtin::detail::betaln_reg);
    engine.registerFunction("ellipj",      &::numkit::builtin::detail::ellipj_reg);
    engine.registerFunction("ellipke",     &::numkit::builtin::detail::ellipke_reg);
    engine.registerFunction("erf",         &::numkit::builtin::detail::erf_reg);
    engine.registerFunction("erfc",        &::numkit::builtin::detail::erfc_reg);
    engine.registerFunction("erfcinv",     &::numkit::builtin::detail::erfcinv_reg);
    engine.registerFunction("erfcx",       &::numkit::builtin::detail::erfcx_reg);
    engine.registerFunction("erfinv",      &::numkit::builtin::detail::erfinv_reg);
    engine.registerFunction("expint",      &::numkit::builtin::detail::expint_reg);
    engine.registerFunction("factor",      &::numkit::builtin::detail::factor_reg);
    engine.registerFunction("factorial",   &::numkit::builtin::detail::factorial_reg);
    engine.registerFunction("gamma",       &::numkit::builtin::detail::gamma_reg);
    engine.registerFunction("gammainc",    &::numkit::builtin::detail::gammainc_reg);
    engine.registerFunction("gammaincinv", &::numkit::builtin::detail::gammaincinv_reg);
    engine.registerFunction("gammaln",     &::numkit::builtin::detail::gammaln_reg);
    engine.registerFunction("gcd",         &::numkit::builtin::detail::gcd_reg);
    engine.registerFunction("isprime",     &::numkit::builtin::detail::isprime_reg);
    engine.registerFunction("lcm",         &::numkit::builtin::detail::lcm_reg);
    engine.registerFunction("legendre",    &::numkit::builtin::detail::legendre_reg);
    engine.registerFunction("nchoosek",    &::numkit::builtin::detail::nchoosek_reg);
    engine.registerFunction("perms",       &::numkit::builtin::detail::perms_reg);
    engine.registerFunction("primes",      &::numkit::builtin::detail::primes_reg);
    engine.registerFunction("psi",         &::numkit::builtin::detail::psi_reg);
}

} // namespace numkit::builtin
