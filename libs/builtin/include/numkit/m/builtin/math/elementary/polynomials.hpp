// libs/builtin/include/numkit/m/builtin/math/elementary/polynomials.hpp
//
// Polynomial-domain builtins. roots now; polyder / polyint / polyval
// later in the round.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::builtin {

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
MValue roots(Allocator &alloc, const MValue &p);

/// polyder(p) — coefficient row of d/dx p(x). For p of length n+1 the
/// derivative has length n.
MValue polyder(Allocator &alloc, const MValue &p);

/// polyder(b, a) — coefficients of d/dx (b(x) / a(x)) as (numerator,
/// denominator). The denominator becomes a^2; numerator is a·b' - b·a'.
std::tuple<MValue, MValue>
polyder(Allocator &alloc, const MValue &b, const MValue &a);

/// polyint(p[, k]) — coefficients of the antiderivative ∫ p(x) dx with
/// integration constant k (default 0). Output length is length(p) + 1.
MValue polyint(Allocator &alloc, const MValue &p, double k = 0.0);

/// tf2zp(b, a) — transfer function H(z) = b(z)/a(z) → (zeros, poles, gain).
/// gain = b(1)/a(1) (leading coefficient ratio).
std::tuple<MValue, MValue, MValue>
tf2zp(Allocator &alloc, const MValue &b, const MValue &a);

/// zp2tf(z, p, k) — zero/pole/gain → (b, a) coefficient rows.
/// b = k · ∏ (x - z); a = ∏ (x - p). Roots may be complex but must
/// come in conjugate pairs (the imaginary residue is dropped — silent
/// non-conjugate input would yield a non-real polynomial; caller is
/// responsible for the pairing).
std::tuple<MValue, MValue>
zp2tf(Allocator &alloc, const MValue &z, const MValue &p, double k);

// ── Curve fitting / evaluation ───────────────────────────────────────

/// Least-squares polynomial fit of degree n. Returns coefficient row vector
/// in descending power order (p[0] * x^n + p[1] * x^(n-1) + ...).
///
/// @throws MError on singular normal-matrix (ill-conditioned) or not enough
///         data points (need at least n+1).
MValue polyfit(Allocator &alloc, const MValue &x, const MValue &y, int n);

/// Horner evaluation of polynomial p at x. Returns array same shape as x.
MValue polyval(Allocator &alloc, const MValue &p, const MValue &x);

} // namespace numkit::m::builtin
