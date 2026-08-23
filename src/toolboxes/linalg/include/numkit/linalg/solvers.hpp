/// @file solvers.hpp
/// @ingroup group_matfun
// toolboxes/linalg/include/numkit/linalg/solvers.hpp
//
// Linear-system solvers: linsolve, lsqminnorm, lsqnonneg.
// Migrated from toolboxes/builtin/src/language/arrays/{matrix,lsq}.cpp.

#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @addtogroup group_matfun
/// @{


/// @brief Solve `A·X = B` (`X = linsolve(A, B)`).
///
/// LU for square `A`, Householder QR for tall `A` (least-squares).
/// Backs `mldivide` / `\` — but `mldivide` lives in toolboxes/builtin (it's
/// the operator implementation). This is the user-facing `linsolve()`.
Value linsolve(const Value &A, const Value &B, std::pmr::memory_resource *mr = nullptr);

/// @brief Minimum-norm least-squares solution (`X = lsqminnorm(A, B)`).
///
/// Computes `pinv(A, tol) · B`. Full-rank A collapses to A\B; rank-
/// deficient A returns the unique min-norm solution.
Value lsqminnorm(const Value &A, const Value &B, bool have_tol, double tol_user,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Result of @ref lsqnonneg_impl.
struct NnlsResult {
    Value x;             ///< Non-negative solution column.
    double resnorm;      ///< Squared 2-norm of the residual `‖C·x − d‖²`.
    Value residual;      ///< Residual column `d − C·x`.
    int exitflag;        ///< 1 on convergence.
    int iterations;      ///< Outer-loop iteration count.
    std::string algorithm; ///< Always "active-set".
    std::string message; ///< Termination message.
};

/// @brief Non-negative least-squares (`x = lsqnonneg(C, d)`).
///
/// Lawson-Hanson active-set NNLS: minimises `‖C·x − d‖₂` subject to
/// `x ≥ 0`. Returns the full @ref NnlsResult for the multi-output forms.
NnlsResult lsqnonneg_impl(const Value &C, const Value &d,
                          std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::linalg
