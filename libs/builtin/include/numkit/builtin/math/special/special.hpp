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

} // namespace numkit::builtin
