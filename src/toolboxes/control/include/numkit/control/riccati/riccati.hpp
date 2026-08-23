/// @file riccati.hpp
/// @ingroup group_control
// toolboxes/control/include/numkit/control/riccati/riccati.hpp
//
// Algebraic Riccati equation solvers.
//
//   care: continuous-time ARE   AᵀX + XA − XBR⁻¹BᵀX + Q = 0
//   dare: discrete-time   ARE   AᵀXA − X − AᵀXB(R + BᵀXB)⁻¹BᵀXA + Q = 0
//
// Both are solved by the matrix sign-function method (no Schur
// ordering): a Newton iteration drives an embedding matrix to its
// sign, whose stable invariant subspace yields X via a small
// least-squares solve. care embeds the Hamiltonian directly; dare
// embeds the symplectic matrix through a Cayley transform that maps the
// unit disk onto the left half-plane so the same sign machinery applies.
//
// Practical for the state dims that show up in textbook control
// problems (n ≲ 32). dare requires a nonsingular A (the explicit
// symplectic form needs A⁻¹); the singular-A pencil path would need a
// QZ solver we do not have yet.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::control {

/// Result of an algebraic Riccati solve, matching MATLAB's `[X, L, G]`
/// output order for `care` / `dare`.
struct RiccatiResult {
    Value X;   ///< Stabilizing solution (symmetric positive-semidefinite).
    Value L;   ///< Closed-loop eigenvalues `eig(A − B·G)` (column, may be complex).
    Value G;   ///< Optimal state-feedback gain (m×n).
};

/// Continuous-time algebraic Riccati equation (`[X, L, G] = care(A, B, Q, R)`).
///
/// Solves @f$ A^{T}X + XA - XBR^{-1}B^{T}X + Q = 0 @f$ for the unique
/// stabilizing @f$ X = X^{T} \succeq 0 @f$ via the matrix sign function
/// of the Hamiltonian @f$ H = \begin{bmatrix} A & -BR^{-1}B^{T} \\ -Q &
/// -A^{T}\end{bmatrix} @f$. The gain is @f$ G = R^{-1}B^{T}X @f$ and the
/// closed-loop poles are @f$ \mathrm{eig}(A - BG) @f$.
///
/// @param A   n×n state matrix.
/// @param B   n×m input matrix.
/// @param Q   n×n state-weight (symmetric PSD).
/// @param R   m×m input-weight (symmetric positive-definite).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `{X, L, G}` (see @ref RiccatiResult).
/// @throws    Error on shape mismatch or a singular Hamiltonian embedding.
/// @see dare, lyap
RiccatiResult care(const Value &A, const Value &B, const Value &Q,
                   const Value &R, std::pmr::memory_resource *mr = nullptr);

/// Discrete-time algebraic Riccati equation (`[X, L, G] = dare(A, B, Q, R)`).
///
/// Solves @f$ A^{T}XA - X - A^{T}XB(R + B^{T}XB)^{-1}B^{T}XA + Q = 0 @f$
/// for the stabilizing @f$ X @f$. Builds the symplectic matrix and
/// applies a Cayley transform (unit disk → left half-plane) so the same
/// sign-function machinery as @ref care selects the stable subspace.
/// The gain is @f$ G = (R + B^{T}XB)^{-1}B^{T}XA @f$, closed-loop poles
/// @f$ \mathrm{eig}(A - BG) @f$ (inside the unit circle).
///
/// @param A   n×n state matrix (**must be nonsingular**).
/// @param B   n×m input matrix.
/// @param Q   n×n state-weight (symmetric PSD).
/// @param R   m×m input-weight (symmetric positive-definite).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `{X, L, G}` (see @ref RiccatiResult).
/// @throws    Error on shape mismatch, singular A, or a singular embedding.
/// @see care, dlyap
RiccatiResult dare(const Value &A, const Value &B, const Value &Q,
                   const Value &R, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
