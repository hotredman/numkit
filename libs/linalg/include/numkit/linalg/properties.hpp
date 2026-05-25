// libs/linalg/include/numkit/linalg/properties.hpp
//
// Scalar properties of a matrix: determinant, trace, rank, condition
// number estimates. Plus the inverse `inv` (returned-matrix form;
// the operator-side `mldivide` (`\`) lives in libs/builtin and uses
// the same underlying `la_solve` kernel).
//
// Migrated from libs/builtin/src/language/arrays/{matrix,linalg_extras}.cpp.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::linalg {

/// @brief Matrix inverse via LU (`B = inv(A)`).
///
/// Prefer @ref linsolve / `\` for solving `A·x = b`; this function
/// exists when the inverse itself is needed as a matrix.
///
/// @param A   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `A^{-1}`.
/// @throws Error  Non-square or singular (`m:inv:singular`).
Value inv(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Trace = sum of diagonal (`t = trace(A)`).
Value trace(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Determinant via LU with partial pivoting (`d = det(A)`).
///
/// `det(A) = sign(P) · prod(diag(U))` where `A = P·L·U`.
Value det(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Numerical rank (`r = rank(A, tol)`).
///
/// Count of singular values above `tol`.
/// Default `tol = max(size(A))·eps(max(svd(A)))`.
Value rank_of(const Value &A, double tol = -1.0, std::pmr::memory_resource *mr = nullptr);

/// @brief 2-norm condition number (`c = cond(A)`).
///
/// `sigma_max / sigma_min` via SVD. Returns `Inf` for singular `A`.
Value cond_2norm(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief 2-norm estimate (`n = normest(A)`).
///
/// Currently equivalent to `svd(A)(1)` — no power-iteration shortcut
/// yet (correctness over performance).
Value normest(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Reciprocal 1-norm condition estimate (`c = rcond(A)`).
///
/// Cheap path: `1 / (norm(A, 1) · norm(inv(A), 1))`. Returns 0 for
/// singular `A`. KNOWN GAP: accurate on well-conditioned cases;
/// differs from LAPACK's `dgecon` on near-singular matrices.
Value rcond(const Value &A, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
