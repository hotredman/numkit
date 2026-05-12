// libs/builtin/include/numkit/builtin/math/poly/polynomials.hpp
//
// Polynomial-domain builtins. roots now; polyder / polyint / polyval
// later in the round.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::builtin {

/// roots(p) — finds the roots of the polynomial whose coefficients are
/// in p (MATLAB convention: p(1) is the leading coefficient, p(end) is
/// the constant term). Returns a column vector of (possibly complex)
/// roots. Uses the shared Durand-Kerner solver.
///
/// Behaviour:
///   * Empty / scalar input → 0×1 column.
///   * Real polynomial → output column is COMPLEX if any root has a
///     non-trivial imaginary part; otherwise DOUBLE.
///   * Trailing zeros in p → corresponding number of roots at 0.
Value roots(const Value &p, std::pmr::memory_resource *mr = nullptr);

/// polyder(p) — coefficient row of d/dx p(x). For p of length n+1 the
/// derivative has length n.
Value polyder(const Value &p, std::pmr::memory_resource *mr = nullptr);

/// polyder(b, a) — coefficients of d/dx (b(x) / a(x)) as (numerator,
/// denominator). The denominator becomes a^2; numerator is a·b' - b·a'.
std::tuple<Value, Value>
polyder(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// polyint(p[, k]) — coefficients of the antiderivative ∫ p(x) dx with
/// integration constant k (default 0). Output length is length(p) + 1.
Value polyint(const Value &p, double k = 0.0, std::pmr::memory_resource *mr = nullptr);

/// tf2zp(b, a) — transfer function H(z) = b(z)/a(z) → (zeros, poles, gain).
/// gain = b(1)/a(1) (leading coefficient ratio).
std::tuple<Value, Value, Value>
tf2zp(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// zp2tf(z, p, k) — zero/pole/gain → (b, a) coefficient rows.
/// b = k · ∏ (x - z); a = ∏ (x - p). Roots may be complex but must
/// come in conjugate pairs (the imaginary residue is dropped — silent
/// non-conjugate input would yield a non-real polynomial; caller is
/// responsible for the pairing).
std::tuple<Value, Value>
zp2tf(const Value &z, const Value &p, double k, std::pmr::memory_resource *mr = nullptr);

// ── Curve fitting / evaluation ───────────────────────────────────────

/// Least-squares polynomial fit of degree n. Returns coefficient row vector
/// in descending power order (p[0] * x^n + p[1] * x^(n-1) + ...).
///
/// @throws Error on singular normal-matrix (ill-conditioned) or not enough
///         data points (need at least n+1).
Value polyfit(const Value &x, const Value &y, int n, std::pmr::memory_resource *mr = nullptr);

/// Horner evaluation of polynomial p at x. Returns array same shape as x.
Value polyval(const Value &p, const Value &x, std::pmr::memory_resource *mr = nullptr);

//// poly(r) — coefficient row of the polynomial whose roots are the
//// elements of r. Returns DOUBLE for real input; COMPLEX-valued roots
//// must be passed as a complex vector (yielding a complex result).
Value poly(const Value &r, std::pmr::memory_resource *mr = nullptr);

/// polyvalm(p, A) — matrix polynomial evaluation: p_0·I + p_1·A +
/// p_2·A² + … (Horner at the matrix level). A must be square.
Value polyvalm(const Value &p, const Value &A, std::pmr::memory_resource *mr = nullptr);

/// polydiv(b, a) — long-division of b(x) by a(x) returning the
/// quotient q and remainder r such that b = q*a + r. Returns the pair.
struct PolyDiv { Value q; Value r; };
PolyDiv polydiv(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

//// padecoef(T, N) — coefficients of the (N,N) Padé approximant of
//// e^{-T·s}. Returns (num, den) row vectors in descending order of s,
//// normalized so the leading denominator coefficient is 1 (matches
//// MATLAB's `[num,den] = padecoef(T,N)`).
struct PadeCoef { Value num; Value den; };
PadeCoef padecoef(double T, int N, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
