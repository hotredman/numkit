// toolboxes/builtin/include/numkit/builtin/math/special/special.hpp
//
// Special functions.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::math {

/// @file
/// @brief Special functions (gamma, erf, beta, Bessel, elliptic, Airy, …).
///
/// All real-only in this revision. Standard-library backed where
/// available (`std::tgamma` / `std::lgamma` / `std::erf` / `std::erfc`);
/// custom asymptotic / series implementations elsewhere.

/// @brief Gamma function (`y = gamma(x)`).
///
/// `Γ(x)`. Backed by `std::tgamma`. Negative integers return Inf
/// or NaN per the standard.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `Γ(x)`, same shape as `x`.
/// @see gammaln, gammainc
Value gammaFn(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Log-absolute gamma (`y = gammaln(x)`).
///
/// `log |Γ(x)|`. Backed by `std::lgamma`. Stable for large `x` where
/// `Γ(x)` would overflow.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `log|Γ(x)|`, same shape as `x`.
/// @see gammaFn
Value gammaln(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Error function (`y = erf(x)`).
///
/// @f$ \text{erf}(x) = \dfrac{2}{\sqrt{\pi}}\,\int_{0}^{x} e^{-t^2}\,dt @f$.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `erf(x)`, same shape as `x`.
/// @see erfc, erfinv
Value erf(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Complementary error function (`y = erfc(x)`).
///
/// `1 - erf(x)`. Numerically accurate for large `x` where `1 - erf(x)`
/// would lose precision.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `erfc(x)`, same shape as `x`.
/// @see erf, erfcx, erfcinv
Value erfc(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse error function (`x = erfinv(y)`).
///
/// Inverse of `erf` on `(-1, 1)`. `y == ±1 → ±Inf`; `|y| > 1`
/// returns `NaN`. Implementation: Winitzki closed-form initial
/// estimate followed by 3 Newton steps.
///
/// @param x   Input array (`y`-values; named `x` for symmetry).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `erfinv(y)`, same shape as `x`.
/// @see erf, erfcinv
Value erfinv(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse complementary error function (`x = erfcinv(y)`).
///
/// `x = erfcinv(y) ⇔ erfc(x) = y`. Domain `y ∈ (0, 2)`. Computed
/// as `erfinv(1 - y)`.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `erfcinv(y)`, same shape as `x`.
/// @see erfc, erfinv
Value erfcinv(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Scaled complementary error function (`y = erfcx(x)`).
///
/// `exp(x²) · erfc(x)`. Numerically stable for large `x` where
/// `erfc(x)` underflows.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `exp(x²) · erfc(x)`, same shape as `x`.
/// @see erfc
Value erfcx(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Beta function (`y = beta(z, w)`).
///
/// `B(z, w) = Γ(z)·Γ(w) / Γ(z + w)`. Computed via `lgamma` to avoid
/// intermediate overflow.
///
/// @param z   First argument.
/// @param w   Second argument.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `B(z, w)`, broadcast shape.
/// @see betaln, betainc
Value beta(const Value &z, const Value &w, std::pmr::memory_resource *mr = nullptr);

/// @brief Log beta (`y = betaln(z, w)`).
///
/// `log B(z, w) = lgamma(z) + lgamma(w) - lgamma(z + w)`.
///
/// @param z   First argument.
/// @param w   Second argument.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `log B(z, w)`, broadcast shape.
/// @see beta
Value betaln(const Value &z, const Value &w, std::pmr::memory_resource *mr = nullptr);

/// @brief Exponential integral (`y = expint(x)`).
///
/// @f$ E_1(x) = \int_x^\infty \dfrac{e^{-t}}{t}\,dt @f$ for `x > 0`.
/// Series expansion for small `x`, asymptotic-continued-fraction for
/// large `x`. Returns `NaN` for `x < 0` (would be complex).
///
/// @param x   Input array (`x > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `E_1(x)`, same shape as `x`.
Value expint(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Digamma function (`y = psi(x)`).
///
/// `ψ(x) = Γ'(x) / Γ(x)`. Recurrence to shift `x >= 6`, then
/// asymptotic series.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `ψ(x)`, same shape as `x`.
/// @see gammaFn
Value psi(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Regularised lower incomplete gamma (`y = gammainc(x, a)`).
///
/// `P(a, x) = γ(a, x) / Γ(a)`. Domain `x >= 0`, `a > 0`.
///
/// @param x   Upper integration limit.
/// @param a   Shape parameter.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `P(a, x)` in `[0, 1]`, broadcast shape.
/// @see gammaincinv, gammaln
Value gammainc(const Value &x, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Regularised incomplete beta (`y = betainc(x, a, b)`).
///
/// `I_x(a, b)`. Domain `x ∈ [0, 1]`, `a > 0`, `b > 0`.
///
/// @param x   Upper integration limit.
/// @param a   First shape parameter.
/// @param b   Second shape parameter.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `I_x(a, b)` in `[0, 1]`, broadcast shape.
/// @see betaincinv, beta
Value betainc(const Value &x, const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Associated Legendre polynomials (`P = legendre(n, x)`).
///
/// Returns an `(n + 1) × length(x)` matrix; row `m + 1` is
/// `P_n^m(x)` (the order-`m` associated Legendre polynomial of
/// degree `n`).
///
/// @param n   Polynomial degree (`n >= 0`).
/// @param x   Evaluation points in `[-1, 1]`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(n + 1) × length(x)` matrix.
Value legendre(int n, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Bessel J (`y = besselj(nu, x)`).
///
/// Bessel function of the first kind `J_ν(x)`.
///
/// @param nu  Order (any real).
/// @param x   Evaluation points.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `J_ν(x)`, broadcast shape.
/// @see bessely, besseli, besselk, besselh
Value besselj(const Value &nu, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Bessel Y (`y = bessely(nu, x)`).
///
/// Bessel function of the second kind `Y_ν(x)`.
///
/// @param nu  Order.
/// @param x   Evaluation points (`x > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `Y_ν(x)`, broadcast shape.
/// @see besselj, besselh
Value bessely(const Value &nu, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Modified Bessel I (`y = besseli(nu, x)`).
///
/// Modified Bessel function of the first kind `I_ν(x)`.
///
/// @param nu  Order.
/// @param x   Evaluation points.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `I_ν(x)`, broadcast shape.
/// @see besselk
Value besseli(const Value &nu, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Modified Bessel K (`y = besselk(nu, x)`).
///
/// Modified Bessel function of the second kind `K_ν(x)`.
///
/// @param nu  Order.
/// @param x   Evaluation points (`x > 0`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `K_ν(x)`, broadcast shape.
/// @see besseli
Value besselk(const Value &nu, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Hankel function (`y = besselh(nu, k, x)`).
///
/// `k = 1 → J_ν + i·Y_ν`; `k = 2 → J_ν - i·Y_ν`. Returns COMPLEX.
///
/// @param nu  Order.
/// @param k   Hankel kind (1 or 2).
/// @param x   Evaluation points.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Complex Hankel function value.
/// @see besselj, bessely
Value besselh(const Value &nu, int k, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Complete elliptic integrals (struct returned by @ref ellipke).
struct EllipKE {
    Value K;  ///< Complete elliptic integral of the first kind `K(m)`.
    Value E;  ///< Complete elliptic integral of the second kind `E(m)`.
};

/// @brief Complete elliptic integrals (`[K, E] = ellipke(m)`).
///
/// `m ∈ [0, 1]`. Computed via the arithmetic-geometric-mean recurrence.
///
/// @param m   Parameter array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `{K, E}` struct, each same shape as `m`.
/// @see ellipj
EllipKE ellipke(const Value &m, std::pmr::memory_resource *mr = nullptr);

/// @brief Airy function family (`y = airy(k, x)`).
///
/// Order convention:
/// - `k = 0` → `Ai(x)`   (the default when `k` is omitted)
/// - `k = 1` → `Ai'(x)`  (derivative)
/// - `k = 2` → `Bi(x)`   (second-kind Airy)
/// - `k = 3` → `Bi'(x)`
///
/// Implemented via `std::cyl_bessel_{j,i,k}` of fractional order
/// `±1/3`, `±2/3` using DLMF §9.6 connection formulas.
///
/// @param k   Variant selector (0–3).
/// @param x   Evaluation points.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Airy values, same shape as `x`.
Value airy(int k, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse regularised lower incomplete gamma
/// (`x = gammaincinv(P, a)`).
///
/// Returns `x` such that `gammainc(x, a) = P`. Domain `P ∈ [0, 1]`,
/// `a > 0`. Newton iteration on `gammainc` with Wilson-Hilferty start.
///
/// @param P   Target probability levels.
/// @param a   Shape parameter.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Inverse value, broadcast shape.
/// @see gammainc
Value gammaincinv(const Value &P, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse regularised incomplete beta
/// (`x = betaincinv(P, a, b)`).
///
/// Returns `x` such that `betainc(x, a, b) = P`. Domain `P ∈ [0, 1]`,
/// `a > 0`, `b > 0`. Newton iteration on `betainc`.
///
/// @param P   Target probability levels.
/// @param a   First shape parameter.
/// @param b   Second shape parameter.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Inverse value, broadcast shape.
/// @see betainc
Value betaincinv(const Value &P, const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Jacobi elliptic outputs (struct returned by @ref ellipj).
struct EllipJ {
    Value sn;  ///< Sine-amplitude.
    Value cn;  ///< Cosine-amplitude.
    Value dn;  ///< Delta-amplitude.
};

/// @brief Jacobi elliptic functions (`[sn, cn, dn] = ellipj(u, m)`).
///
/// `m ∈ [0, 1]`. Descending Landen / AGM transformation
/// (Abramowitz & Stegun 16.4).
///
/// @param u   Amplitude argument.
/// @param m   Parameter array (`m ∈ [0, 1]`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `{sn, cn, dn}` struct, each broadcast shape.
/// @see ellipke
EllipJ ellipj(const Value &u, const Value &m, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::math
