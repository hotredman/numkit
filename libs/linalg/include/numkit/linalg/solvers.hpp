// libs/linalg/include/numkit/linalg/solvers.hpp
//
// Linear-system solvers: linsolve, lsqminnorm, lsqnonneg.
// Migrated from libs/builtin/src/language/arrays/{matrix,lsq}.cpp.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::linalg {

/// @brief Solve `A·X = B` (`X = linsolve(A, B)`).
///
/// LU for square `A`, Householder QR for tall `A` (least-squares).
/// Backs `mldivide` / `\` — but `mldivide` lives in libs/builtin (it's
/// the operator implementation). This is the user-facing `linsolve()`.
Value linsolve(const Value &A, const Value &B, std::pmr::memory_resource *mr = nullptr);

/// @brief Minimum-norm least-squares solution (`X = lsqminnorm(A, B)`).
///
/// Computes `pinv(A, tol) · B`. Full-rank A collapses to A\B; rank-
/// deficient A returns the unique min-norm solution.
Value lsqminnorm(const Value &A, const Value &B, bool have_tol, double tol_user,
                 std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
