// src/builtin/include/numkit/builtin/specfun.hpp
//
// Pure C++ Special mathematical functions (gamma, beta, bessel, erf, combinatorics).
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @file
/// @brief Special mathematical functions (gamma, beta, erf, Bessel, Airy, combinatorics).
///
/// Provides a clean, engine-free C++ API for special functions across analysis,
/// number theory, combinatorics, and mathematical physics.

// ── Gamma & Beta Functions ──────────────────────────────────────────────────

/// @brief Complete gamma function (`Γ(x)`).
/// @param x Input value or array.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array containing gamma function values.
/// @see gammaln, gammainc, psi
Value gamma(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Logarithm of the absolute value of the gamma function (`ln|Γ(x)|`).
/// @param x Input value or array.
/// @param mr Memory resource.
/// @return Array containing log-gamma values.
/// @see gamma, psi
Value gammaln(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Incomplete gamma function (`P(a, x)`).
/// @param x Upper limit of integration.
/// @param a Scaling parameter.
/// @param mr Memory resource.
/// @return Regularized lower incomplete gamma values.
/// @see gamma, gammaincinv
Value gammainc(const Value &x, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse incomplete gamma function.
/// @param y Function value in [0, 1].
/// @param a Scaling parameter.
/// @param mr Memory resource.
/// @return Value `x` such that `gammainc(x, a) == y`.
/// @see gammainc
Value gammaincinv(const Value &y, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Digamma / polygamma function (`ψ(x) = d/dx ln Γ(x)`).
/// @param x Input value or array.
/// @param mr Memory resource.
/// @return Logarithmic derivative of gamma function.
/// @see gamma, gammaln
Value psi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Beta function (`B(z, w) = Γ(z)Γ(w) / Γ(z+w)`).
/// @param z First parameter.
/// @param w Second parameter.
/// @param mr Memory resource.
/// @return Beta function values.
/// @see betaln, betainc
Value beta(const Value &z, const Value &w, std::pmr::memory_resource *mr = nullptr);

/// @brief Natural logarithm of the beta function (`ln B(z, w)`).
/// @param z First parameter.
/// @param w Second parameter.
/// @param mr Memory resource.
/// @return Log-beta values.
/// @see beta
Value betaln(const Value &z, const Value &w, std::pmr::memory_resource *mr = nullptr);

/// @brief Incomplete beta function (`I_x(z, w)`).
/// @param x Upper limit in [0, 1].
/// @param z First parameter.
/// @param w Second parameter.
/// @param mr Memory resource.
/// @return Regularized incomplete beta values.
/// @see beta, betaincinv
Value betainc(const Value &x, const Value &z, const Value &w, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse incomplete beta function.
/// @param y Value in [0, 1].
/// @param z First parameter.
/// @param w Second parameter.
/// @param mr Memory resource.
/// @return Value `x` such that `betainc(x, z, w) == y`.
/// @see betainc
Value betaincinv(const Value &y, const Value &z, const Value &w, std::pmr::memory_resource *mr = nullptr);

// ── Error Functions ─────────────────────────────────────────────────────────

/// @brief Error function (`erf(x) = 2/√π ∫_0^x e^(-t^2) dt`).
/// @param x Input value or array.
/// @param mr Memory resource.
/// @return Error function values.
/// @see erfc, erfinv
Value erf(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Complementary error function (`erfc(x) = 1 - erf(x)`).
/// @param x Input value or array.
/// @param mr Memory resource.
/// @return Complementary error function values.
/// @see erf, erfcx
Value erfc(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Scaled complementary error function (`erfcx(x) = exp(x^2) * erfc(x)`).
/// @param x Input value or array.
/// @param mr Memory resource.
/// @return Scaled complementary error values.
/// @see erfc
Value erfcx(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse error function (`erfinv(y)`).
/// @param x Value in (-1, 1).
/// @param mr Memory resource.
/// @return Value `y` such that `erf(y) == x`.
/// @see erf, erfcinv
Value erfinv(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse complementary error function (`erfcinv(y)`).
/// @param x Value in (0, 2).
/// @param mr Memory resource.
/// @return Value `y` such that `erfc(y) == x`.
/// @see erfc, erfinv
Value erfcinv(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Bessel & Airy Functions ─────────────────────────────────────────────────

/// @brief Bessel function of the first kind (`J_ν(z)`).
/// @param nu Order of the Bessel function.
/// @param z Argument.
/// @param mr Memory resource.
/// @return Bessel function values.
/// @see bessely, besseli, besselk
Value besselj(const Value &nu, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Bessel function of the second kind (Neumann function `Y_ν(z)`).
/// @param nu Order of the Bessel function.
/// @param z Argument.
/// @param mr Memory resource.
/// @return Bessel function values.
/// @see besselj
Value bessely(const Value &nu, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Modified Bessel function of the first kind (`I_ν(z)`).
/// @param nu Order.
/// @param z Argument.
/// @param mr Memory resource.
/// @return Modified Bessel function values.
/// @see besselk
Value besseli(const Value &nu, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Modified Bessel function of the second kind (`K_ν(z)`).
/// @param nu Order.
/// @param z Argument.
/// @param mr Memory resource.
/// @return Modified Bessel function values.
/// @see besseli
Value besselk(const Value &nu, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Bessel function of the third kind (Hankel function `H_ν^(k)(z)`).
/// @param nu Order.
/// @param k Kind index (1 or 2).
/// @param z Argument.
/// @param mr Memory resource.
/// @return Hankel function values.
/// @see besselj, bessely
Value besselh(const Value &nu, int k, const Value &z, std::pmr::memory_resource *mr = nullptr);

struct EllipKE {
    Value K;
    Value E;
};

struct EllipJ {
    Value sn;
    Value cn;
    Value dn;
};

/// @brief Airy function (`Ai(z)` or `k`-th derivative / `Bi`).
/// @param k Function kind / derivative index (0: Ai, 1: Ai', 2: Bi, 3: Bi').
/// @param z Argument.
/// @param mr Memory resource.
/// @return Airy function values.
Value airy(int k, const Value &z, std::pmr::memory_resource *mr = nullptr);
Value airy(const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief Exponential integral function (`E_1(x)`).
/// @param x Argument.
/// @param mr Memory resource.
/// @return Exponential integral values.
Value expint(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Complete elliptic integrals of the first and second kind (`[K, E] = ellipke(m)`).
/// @param m Parameter `m = k^2`.
/// @param mr Memory resource.
/// @return Complete elliptic integral values.
EllipKE ellipke(const Value &m, std::pmr::memory_resource *mr = nullptr);

/// @brief Jacobi elliptic functions (`[sn, cn, dn] = ellipj(u, m)`).
/// @param u Argument.
/// @param m Parameter.
/// @param mr Memory resource.
/// @return Jacobi elliptic function values.
EllipJ ellipj(const Value &u, const Value &m, std::pmr::memory_resource *mr = nullptr);

/// @brief Associated Legendre functions (`P_n^m(x)`).
/// @param n Degree of the Legendre function.
/// @param x Argument.
/// @param mr Memory resource.
/// @return Associated Legendre values.
Value legendre(int n, const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Combinatorics & Number Theory ───────────────────────────────────────────

/// @brief Factorial of non-negative integer elements (`n!`).
/// @param n Non-negative integers.
/// @param mr Memory resource.
/// @return Factorials.
/// @see gamma, nchoosek
Value factorial(const Value &n, std::pmr::memory_resource *mr = nullptr);

/// @brief Binomial coefficient or all combinations (`n choose k`).
/// @param v Scalar `n` or vector of elements.
/// @param k Subset size.
/// @param mr Memory resource.
/// @return Binomial coefficient or combination matrix.
Value primes(double n, std::pmr::memory_resource *mr = nullptr);
Value primes(const Value &n, std::pmr::memory_resource *mr = nullptr);
Value isprime(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value factor(double n, std::pmr::memory_resource *mr = nullptr);
Value factor(const Value &n, std::pmr::memory_resource *mr = nullptr);

Value nchoosek(double n, double k, std::pmr::memory_resource *mr = nullptr);
Value nchoosekCombinations(const Value &v, double kd, std::pmr::memory_resource *mr = nullptr);
Value nchoosek(const Value &v, int k, std::pmr::memory_resource *mr = nullptr);
Value nchoosek(const Value &v, const Value &k, std::pmr::memory_resource *mr = nullptr);

/// @brief All permutations of a vector elements.
/// @param v Input vector.
/// @param mr Memory resource.
/// @return Matrix of all `n!` permutations.
/// @see nchoosek, randperm
Value perms(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Greatest common divisor of two integers or arrays.
/// @param a First operand.
/// @param b Second operand.
/// @param mr Memory resource.
/// @return Greatest common divisor.
/// @see lcm
Value gcd(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
Value lcm(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

Value colperm(const Value &s, std::pmr::memory_resource *mr = nullptr);
Value symrcm(const Value &s, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
