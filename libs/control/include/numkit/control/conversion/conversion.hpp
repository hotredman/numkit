// libs/control/include/numkit/control/conversion/conversion.hpp
//
// Inter-form converters between tf / zpk / ss representations. These
// are the matrix forms that operate directly on the coefficient
// arrays (so a script can use them either with raw num/den/A/B/C/D
// or with struct-tagged tf/zpk/ss values).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <utility>

namespace numkit::control {

/// Result of @ref tf2zp — MATLAB `[z, p, k] = tf2zp(num, den)`.
struct Tf2ZpResult {
    Value z;   ///< Zeros (column vector, complex if needed).
    Value p;   ///< Poles (column vector, complex if needed).
    Value k;   ///< Gain (scalar = num[0] / den[0] after stripping leading zeros).
};

/// State-space realisation `(A, B, C, D)`, the result of @ref tf2ss
/// (MATLAB `[A, B, C, D] = tf2ss(...)`). For an n-th order proper
/// rational input: A is n×n, B is n×1, C is 1×n, D is 1×1.
struct StateSpace {
    Value A;
    Value B;
    Value C;
    Value D;
};

/// Convert tf coefficients to zero-pole-gain form.
///
/// Equivalent to MATLAB's `[z, p, k] = tf2zp(num, den)`. Roots are
/// computed via @ref builtin::roots so complex conjugate pairs and
/// trailing-zero roots are handled exactly as MATLAB does.
///
/// @param num  Numerator polynomial (row, descending powers).
/// @param den  Denominator polynomial (leading coefficient must be nonzero).
/// @param mr   Memory resource (nullptr → process default).
/// @return     @ref Tf2ZpResult with `z`, `p`, `k`.
/// @throws     Error if the denominator's leading coefficient is zero.
///
/// @code
/// auto r = tf2zp({2, -16, 30}, {1, -1});  // (2s² − 16s + 30) / (s − 1)
/// // r.z = [3; 5], r.p = [1], r.k = 2.
/// @endcode
///
/// @see zp2tf, tf2ss
Tf2ZpResult tf2zp(const Value &num, const Value &den,
                  std::pmr::memory_resource *mr = nullptr);

/// Convert zero-pole-gain form to tf coefficients.
///
/// Equivalent to MATLAB's `[num, den] = zp2tf(z, p, k)`. Expands
/// `num = k · ∏(s − z_i)` and `den = ∏(s − p_i)` via @ref builtin::poly.
/// The result preserves complex coefficients if the inputs are complex.
///
/// @param z   Zeros (column or row vector).
/// @param p   Poles (column or row vector).
/// @param k   Gain (scalar).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(num, den)`; bind via `auto [num, den] = zp2tf(z, p, k);`.
///
/// @see tf2zp, tf2ss
std::pair<Value, Value>
zp2tf(const Value &z, const Value &p, const Value &k,
      std::pmr::memory_resource *mr = nullptr);

/// Convert tf coefficients to controllable canonical state-space.
///
/// Equivalent to MATLAB's `[A, B, C, D] = tf2ss(num, den)`. Produces
/// the controllable canonical realisation (companion-matrix form):
/// @f[
///   A = \begin{pmatrix}
///         0 & 1 & 0 & \cdots & 0 \\
///         \vdots & & & & \\
///         0 & 0 & \cdots & 0 & 1 \\
///         -a_n & -a_{n-1} & \cdots & & -a_1
///       \end{pmatrix},\quad
///   B = \begin{pmatrix}0 \\ \vdots \\ 0 \\ 1\end{pmatrix}
/// @f]
/// with `C[k] = b[n − k] − b[0]·a[n − k]` and `D = b[0]`. The numerator
/// must be no longer than the denominator (proper rational).
///
/// @param num  Numerator (length ≤ length(den)).
/// @param den  Denominator (leading coefficient nonzero).
/// @param mr   Memory resource (nullptr → process default).
/// @return     @ref StateSpace `{A, B, C, D}`.
/// @throws     Error if `length(num) > length(den)` (improper).
///
/// @see ss2tf, tf2zp
StateSpace tf2ss(const Value &num, const Value &den,
                 std::pmr::memory_resource *mr = nullptr);

/// Convert state-space to tf coefficients.
///
/// Equivalent to MATLAB's `[num, den] = ss2tf(A, B, C, D, iu)`. Uses
/// the Faddeev–LeVerrier expansion to build
/// @f$ H(s) = C\,(sI - A)^{-1}\,B + D @f$ without ever forming the
/// matrix inverse. The output denominator is the characteristic
/// polynomial @f$ \det(sI - A) @f$.
///
/// `iu` selects the input column when B has multiple inputs (1-based).
/// Only single-output systems (C with 1 row) are supported; MIMO
/// would need a generalized eigenproblem and is not yet exposed.
///
/// @param A   n×n state matrix.
/// @param B   n×nin input matrix.
/// @param C   1×n output row.
/// @param D   1×nin feedthrough.
/// @param iu  Input column to use (1-based; default 1 in MATLAB).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(num, den)`; bind via `auto [num, den] = ss2tf(...);`.
/// @throws    Error if A is non-square, `iu` is out of range, or
///            C has more than 1 row (MIMO not supported).
///
/// @see tf2ss, tf2zp
std::pair<Value, Value>
ss2tf(const Value &A, const Value &B,
      const Value &C, const Value &D, int iu,
      std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
