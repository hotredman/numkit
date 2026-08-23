// include/numkit/builtin/specfun.hpp
//
// Special mathematical functions (gamma, beta, bessel, erf, combinatorics).
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit {
class Engine;
}

namespace numkit::builtin {

/// @file
/// @brief Special mathematical functions (gamma, beta, erf, Bessel, Airy, combinatorics).

// ── Gamma & Beta Functions ──────────────────────────────────────────────────

/// @brief Complete gamma function (`Γ(x)`).
Value gamma(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Logarithm of the absolute value of the gamma function (`ln|Γ(x)|`).
Value gammaln(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Incomplete gamma function.
Value gammainc(const Value &x, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse incomplete gamma function.
Value gammaincinv(const Value &y, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Digamma / polygamma function (`ψ(x)`).
Value psi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Beta function (`B(z, w)`).
Value beta(const Value &z, const Value &w, std::pmr::memory_resource *mr = nullptr);

/// @brief Logarithm of the beta function (`ln B(z, w)`).
Value betaln(const Value &z, const Value &w, std::pmr::memory_resource *mr = nullptr);

/// @brief Incomplete beta function.
Value betainc(const Value &x, const Value &z, const Value &w, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse incomplete beta function.
Value betaincinv(const Value &y, const Value &z, const Value &w, std::pmr::memory_resource *mr = nullptr);

// ── Error Functions ─────────────────────────────────────────────────────────

/// @brief Error function (`erf(x)`).
Value erf(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Complementary error function (`erfc(x) = 1 - erf(x)`).
Value erfc(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Scaled complementary error function (`exp(x^2) * erfc(x)`).
Value erfcx(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse error function.
Value erfinv(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse complementary error function.
Value erfcinv(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Bessel & Airy Functions ─────────────────────────────────────────────────

/// @brief Bessel function of the first kind (`J_ν(z)`).
Value besselj(const Value &nu, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Bessel function of the second kind (`Y_ν(z)`).
Value bessely(const Value &nu, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Modified Bessel function of the first kind (`I_ν(z)`).
Value besseli(const Value &nu, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Modified Bessel function of the second kind (`K_ν(z)`).
Value besselk(const Value &nu, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Bessel function of the third kind (Hankel function `H_ν^(k)(z)`).
Value besselh(const Value &nu, int k, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Airy function (`Ai(z)`).
Value airy(const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Exponential integral function (`E_1(x)`).
Value expint(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Complete elliptic integrals of first and second kind.
Value ellipke(const Value &m, std::pmr::memory_resource *mr = nullptr);

/// @brief Associated Legendre functions.
Value legendre(int n, const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Combinatorics & Number Theory ───────────────────────────────────────────

/// @brief Factorial of input elements.
Value factorial(const Value &n, std::pmr::memory_resource *mr = nullptr);

/// @brief Binomial coefficient or all combinations.
Value nchoosek(const Value &v, int k, std::pmr::memory_resource *mr = nullptr);

/// @brief All permutations of a vector.
Value perms(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Greatest common divisor.
Value gcd(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Least common multiple.
Value lcm(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

// ── Registration ────────────────────────────────────────────────────────────

/// @brief Registers all special mathematical builtins into the engine instance.
void register_specfun(Engine &engine);

} // namespace numkit::builtin
