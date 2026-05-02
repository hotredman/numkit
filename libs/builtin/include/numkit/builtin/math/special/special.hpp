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
Value gammaFn(std::pmr::memory_resource *mr, const Value &x);

/// gammaln(x) — log|Γ(x)|. Real-only; lgamma backed.
Value gammaln(std::pmr::memory_resource *mr, const Value &x);

/// erf(x)     — error function 2/√π ∫₀ˣ e^{-t²} dt.
Value erf(std::pmr::memory_resource *mr, const Value &x);

/// erfc(x)    — 1 - erf(x), accurate for large x.
Value erfc(std::pmr::memory_resource *mr, const Value &x);

/// erfinv(y)  — inverse error function on (-1, 1).
///              y == ±1 → ±Inf, |y| > 1 (real input) → NaN.
///              Implementation: Winitzki's closed-form initial estimate
///              followed by 3 Newton steps for full double precision.
Value erfinv(std::pmr::memory_resource *mr, const Value &x);

/// erfcinv(y) — inverse complementary error function: x = erfcinv(y)
///              ⇔ erfc(x) == y. Domain (0, 2). Computed as
///              erfinv(1 - y).
Value erfcinv(std::pmr::memory_resource *mr, const Value &x);

/// erfcx(x)   — scaled complementary error function exp(x²) · erfc(x).
///              Numerically stable for large x where erfc(x) underflows.
Value erfcx(std::pmr::memory_resource *mr, const Value &x);

// ── Pack 19: extra special functions ─────────────────────────────────
/// beta(z, w)    — B(z, w) = Γ(z)Γ(w)/Γ(z+w). Computed via lgamma to
/// avoid overflow.
Value beta(std::pmr::memory_resource *mr, const Value &z, const Value &w);
/// betaln(z, w)  — log B(z, w) = lgamma(z) + lgamma(w) - lgamma(z+w).
Value betaln(std::pmr::memory_resource *mr, const Value &z, const Value &w);
/// expint(x)     — E1(x) = ∫_x^∞ e^{-t}/t dt for x > 0. Series for
/// small x, asymptotic continued-fraction-style for large x. Returns
/// NaN for x < 0 (would be complex).
Value expint(std::pmr::memory_resource *mr, const Value &x);
/// psi(x)        — digamma function ψ(x) = Γ'(x)/Γ(x). Recurrence to
/// shift x ≥ 6, then asymptotic series.
Value psi(std::pmr::memory_resource *mr, const Value &x);

// ── Pack 26: incomplete gamma / beta / Legendre ──────────────────────
/// gammainc(x, a) — regularized lower incomplete gamma P(a, x) =
/// γ(a, x)/Γ(a). x ≥ 0, a > 0.
Value gammainc(std::pmr::memory_resource *mr, const Value &x, const Value &a);
/// betainc(x, a, b) — regularized incomplete beta I_x(a, b).
Value betainc(std::pmr::memory_resource *mr, const Value &x,
              const Value &a, const Value &b);
/// legendre(n, x) — associated Legendre polynomials. Returns an
/// (n+1) × length(x) matrix; row m+1 is P_n^m(x).
Value legendre(std::pmr::memory_resource *mr, int n, const Value &x);

// ── Pack 27: Bessel functions (C++17 std::cyl_*) ─────────────────────
/// besselj(nu, x) — Bessel function of the first kind J_ν(x).
Value besselj(std::pmr::memory_resource *mr, const Value &nu, const Value &x);
/// bessely(nu, x) — Bessel of the second kind Y_ν(x).
Value bessely(std::pmr::memory_resource *mr, const Value &nu, const Value &x);
/// besseli(nu, x) — modified Bessel of the first kind I_ν(x).
Value besseli(std::pmr::memory_resource *mr, const Value &nu, const Value &x);
/// besselk(nu, x) — modified Bessel of the second kind K_ν(x).
Value besselk(std::pmr::memory_resource *mr, const Value &nu, const Value &x);

// ── Pack 28: Hankel + elliptic integrals ─────────────────────────────
/// besselh(nu, k, x) — Hankel function. k == 1 → J_ν + i·Y_ν;
/// k == 2 → J_ν − i·Y_ν. Returns complex.
Value besselh(std::pmr::memory_resource *mr,
              const Value &nu, int k, const Value &x);

/// ellipke(m) — complete elliptic integrals of the first (K) and
/// second (E) kind. m ∈ [0, 1]. Returns [K, E] via the (n+2)-arg
/// adapter (multi-output) — public API returns a pair.
struct EllipKE { Value K; Value E; };
EllipKE ellipke(std::pmr::memory_resource *mr, const Value &m);

// ── Pack 36: airy ────────────────────────────────────────────────────
/// airy(k, x) — Airy function family per MATLAB:
///   k = 0 → Ai(x)         (default in MATLAB if k omitted)
///   k = 1 → Ai'(x)        (derivative)
///   k = 2 → Bi(x)         (second-kind Airy)
///   k = 3 → Bi'(x)
/// Implemented via std::cyl_bessel_{j,i,k} of fractional order
/// ±1/3, ±2/3 using the connection formulas in DLMF §9.6.
/// Vectorizes over `x`; `k` must be a scalar in {0,1,2,3}.
Value airy(std::pmr::memory_resource *mr, int k, const Value &x);

/// gammaincinv(P, a) — inverse of regularized lower incomplete gamma:
/// returns x such that gammainc(x, a) == P. Domain: P ∈ [0,1], a > 0.
/// Newton iteration on `gammainc` with a Wilson-Hilferty starting point.
Value gammaincinv(std::pmr::memory_resource *mr, const Value &P, const Value &a);

/// betaincinv(P, a, b) — inverse of regularized incomplete beta:
/// returns x such that betainc(x, a, b) == P. Domain: P ∈ [0,1],
/// a > 0, b > 0. Newton iteration on `betainc`.
Value betaincinv(std::pmr::memory_resource *mr, const Value &P,
                 const Value &a, const Value &b);

/// ellipj(u, m) — Jacobi elliptic functions sn, cn, dn. m ∈ [0, 1].
/// Implemented via the descending Landen / arithmetic-geometric-mean
/// transformation (Abramowitz & Stegun 16.4).
struct EllipJ { Value sn; Value cn; Value dn; };
EllipJ ellipj(std::pmr::memory_resource *mr, const Value &u, const Value &m);

} // namespace numkit::builtin
