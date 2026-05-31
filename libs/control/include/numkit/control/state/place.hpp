// libs/control/include/numkit/control/state/place.hpp
//
// Pole-placement state-feedback design via Ackermann's formula.
// SISO only — the multi-input `place` variant uses Kautsky–Nichols
// eigenvector assignment, which we don't support here.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::control {

/// Ackermann's pole placement (`K = acker(A, B, p)`).
///
/// Computes the SISO state-feedback gain row K such that
/// `eig(A − B·K) = p`. Implements:
///   @f[ K = [0\ 0\ \cdots\ 0\ 1]\ \mathcal{C}(A,B)^{-1}\ \phi(A) @f]
/// where @f$ \mathcal{C} @f$ is the controllability matrix and
/// @f$ \phi(s) = \prod_i (s - p_i) @f$ is the desired characteristic
/// polynomial.
///
/// Single-input only. For multi-input systems use a Riccati-based
/// design (`lqr`) or split into multiple SISO loops.
///
/// @param A   n×n state matrix.
/// @param B   n×1 input column.
/// @param p   n-element vector of desired closed-loop poles.
/// @param mr  Memory resource (nullptr → process default).
/// @return    1×n state-feedback gain row K.
/// @throws    Error if B has more than one column, dimensions
///            disagree, or `(A, B)` is uncontrollable.
///
/// @code
/// auto K = acker(A, B, Value{-2, -3, -4});
/// // eig(A - B*K) ≈ [-2; -3; -4]
/// @endcode
///
/// @see place
Value acker(const Value &A, const Value &B, const Value &p,
            std::pmr::memory_resource *mr = nullptr);

/// @brief SISO alias for @ref acker — `K = place(A, B, p)`.
///
/// The robust multi-input variant (Kautsky–Nichols eigenvector
/// assignment) is intentionally not re-implemented here; calling
/// `place` on a multi-input system therefore behaves identically to
/// the SISO Ackermann path and will throw on `B` with more than one
/// column.
///
/// @param A   n×n state matrix.
/// @param B   n×1 input column.
/// @param p   n-element vector of desired closed-loop poles.
/// @param mr  Memory resource (nullptr → process default).
/// @return    1×n state-feedback gain row K.
/// @throws    Error if B has more than one column, dimensions
///            disagree, or `(A, B)` is uncontrollable.
/// @see acker
Value place(const Value &A, const Value &B, const Value &p,
            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
