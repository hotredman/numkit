// libs/builtin/include/numkit/builtin/math/special/special.hpp
//
// Special functions: gamma, gammaln, erf, erfc, erfinv. Real-only;
// std::tgamma / std::lgamma / std::erf / std::erfc backed. erfinv is
// Winitzki + 3 Newton steps.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// gamma(x)   — Γ(x). Real-only; tgamma backed.
Value gammaFn(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// gammaln(x) — log|Γ(x)|. Real-only; lgamma backed.
Value gammaln(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// erf(x)     — error function 2/√π ∫₀ˣ e^{-t²} dt.
Value erf(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// erfc(x)    — 1 - erf(x), accurate for large x.
Value erfc(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// erfinv(y)  — inverse error function on (-1, 1).
///              y == ±1 → ±Inf, |y| > 1 (real input) → NaN.
///              Implementation: Winitzki's closed-form initial estimate
///              followed by 3 Newton steps for full double precision.
Value erfinv(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// erfcinv(y) — inverse complementary error function: x = erfcinv(y)
///              ⇔ erfc(x) == y. Domain (0, 2). Computed as
///              erfinv(1 - y).
Value erfcinv(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// erfcx(x)   — scaled complementary error function exp(x²) · erfc(x).
///              Numerically stable for large x where erfc(x) underflows.
Value erfcx(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Pack 19: extra special functions ─────────────────────────────────
/// beta(z, w)    — B(z, w) = Γ(z)Γ(w)/Γ(z+w). Computed via lgamma to
/// avoid overflow.
Value beta(const Value &z, const Value &w, std::pmr::memory_resource *mr = nullptr);
/// betaln(z, w)  — log B(z, w) = lgamma(z) + lgamma(w) - lgamma(z+w).
Value betaln(const Value &z, const Value &w, std::pmr::memory_resource *mr = nullptr);
/// expint(x)     — E1(x) = ∫_x^∞ e^{-t}/t dt for x > 0. Series for
/// small x, asymptotic continued-fraction-style for large x. Returns
/// NaN for x < 0 (would be complex).
Value expint(const Value &x, std::pmr::memory_resource *mr = nullptr);
/// psi(x)        — digamma function ψ(x) = Γ'(x)/Γ(x). Recurrence to
/// shift x ≥ 6, then asymptotic series.
Value psi(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Pack 26: incomplete gamma / beta / Legendre ──────────────────────
/// gammainc(x, a) — regularized lower incomplete gamma P(a, x) =
/// γ(a, x)/Γ(a). x ≥ 0, a > 0.
Value gammainc(const Value &x, const Value &a, std::pmr::memory_resource *mr = nullptr);
/// betainc(x, a, b) — regularized incomplete beta I_x(a, b).
Value betainc(const Value &x, const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
/// legendre(n, x) — associated Legendre polynomials. Returns an
/// (n+1) × length(x) matrix; row m+1 is P_n^m(x).
Value legendre(int n, const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Pack 27: Bessel functions (C++17 std::cyl_*) ─────────────────────
/// besselj(nu, x) — Bessel function of the first kind J_ν(x).
Value besselj(const Value &nu, const Value &x, std::pmr::memory_resource *mr = nullptr);
/// bessely(nu, x) — Bessel of the second kind Y_ν(x).
Value bessely(const Value &nu, const Value &x, std::pmr::memory_resource *mr = nullptr);
/// besseli(nu, x) — modified Bessel of the first kind I_ν(x).
Value besseli(const Value &nu, const Value &x, std::pmr::memory_resource *mr = nullptr);
/// besselk(nu, x) — modified Bessel of the second kind K_ν(x).
Value besselk(const Value &nu, const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Pack 28: Hankel + elliptic integrals ─────────────────────────────
/// besselh(nu, k, x) — Hankel function. k == 1 → J_ν + i·Y_ν;
/// k == 2 → J_ν − i·Y_ν. Returns complex.
Value besselh(const Value &nu, int k, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// ellipke(m) — complete elliptic integrals of the first (K) and
/// second (E) kind. m ∈ [0, 1]. Returns [K, E] via the (n+2)-arg
/// adapter (multi-output) — public API returns a pair.
struct EllipKE { Value K; Value E; };
EllipKE ellipke(const Value &m, std::pmr::memory_resource *mr = nullptr);

// ── Pack 36: airy ────────────────────────────────────────────────────
/// airy(k, x) — Airy function family per MATLAB:
///   k = 0 → Ai(x)         (default in MATLAB if k omitted)
///   k = 1 → Ai'(x)        (derivative)
///   k = 2 → Bi(x)         (second-kind Airy)
///   k = 3 → Bi'(x)
/// Implemented via std::cyl_bessel_{j,i,k} of fractional order
/// ±1/3, ±2/3 using the connection formulas in DLMF §9.6.
/// Vectorizes over `x`; `k` must be a scalar in {0,1,2,3}.
Value airy(int k, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// gammaincinv(P, a) — inverse of regularized lower incomplete gamma:
/// returns x such that gammainc(x, a) == P. Domain: P ∈ [0,1], a > 0.
/// Newton iteration on `gammainc` with a Wilson-Hilferty starting point.
Value gammaincinv(const Value &P, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// betaincinv(P, a, b) — inverse of regularized incomplete beta:
/// returns x such that betainc(x, a, b) == P. Domain: P ∈ [0,1],
/// a > 0, b > 0. Newton iteration on `betainc`.
Value betaincinv(const Value &P, const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// ellipj(u, m) — Jacobi elliptic functions sn, cn, dn. m ∈ [0, 1].
/// Implemented via the descending Landen / arithmetic-geometric-mean
/// transformation (Abramowitz & Stegun 16.4).
struct EllipJ { Value sn; Value cn; Value dn; };
EllipJ ellipj(const Value &u, const Value &m, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
