/// @file lyapunov.hpp
/// @ingroup group_control
// toolboxes/control/include/numkit/control/lyapunov/lyapunov.hpp
//
// Continuous and discrete Lyapunov equations.
//
//   lyap (A, Q): A·X + X·Aᵀ + Q = 0
//   dlyap(A, Q): A·X·Aᵀ − X + Q = 0
//
// We solve the n²×n² vectorised system directly — fine for the
// state dims that show up in textbook control problems (n ≤ 32).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::control {

/// Continuous-time Lyapunov equation solver (`X = lyap(A, Q)`).
///
/// Solves @f$ A X + X A^{T} + Q = 0 @f$ by vectorising the equation
/// (Kronecker form) into a dense n²×n² linear system and applying
/// Gaussian elimination. Practical for n up to ~32; for larger
/// problems a Bartels–Stewart implementation would be wanted.
///
/// @param A   n×n state matrix.
/// @param Q   n×n right-hand side (typically symmetric PSD).
/// @param mr  Memory resource (nullptr → process default).
/// @return    n×n solution X.
/// @throws    Error if dimensions disagree or the Kronecker system is
///            singular (A has an eigenvalue λ with λ + λ* = 0).
///
/// @code
/// auto X = lyap(A, Q);
/// // For stable A and Q = Qᵀ > 0, X is symmetric positive definite.
/// @endcode
///
/// @see dlyap
Value lyap(const Value &A, const Value &Q,
           std::pmr::memory_resource *mr = nullptr);

/// Discrete-time Lyapunov (Stein) equation solver (`X = dlyap(A, Q)`).
///
/// Solves @f$ A X A^{T} - X + Q = 0 @f$ via the same Kronecker
/// approach as @ref lyap.
///
/// @param A   n×n state matrix (typically Schur-stable, |λ_i| < 1).
/// @param Q   n×n right-hand side.
/// @param mr  Memory resource (nullptr → process default).
/// @return    n×n solution X.
/// @throws    Error if dimensions disagree or the Kronecker system is
///            singular (A has eigenvalues λ_i, λ_j with λ_i·λ_j = 1).
///
/// @see lyap
Value dlyap(const Value &A, const Value &Q,
            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
