// libs/linalg/include/numkit/linalg/decompositions.hpp
//
// Classical matrix factorisations: Cholesky, LU, QR, SVD.
// Migrated from libs/builtin/src/language/arrays/matrix.cpp.

#pragma once

#include <memory_resource>
#include <tuple>
#include <numkit/core/value.hpp>

namespace numkit::linalg {

/// @brief Cholesky factorisation (`R = chol(A)`).
///
/// Returns the upper-triangular `R` such that `R' · R == A`.
///
/// @param A   Symmetric positive-definite matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Upper-triangular factor `R`.
/// @throws Error  Non-square or not positive-definite (`m:chol:notPosDef`).
Value chol(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief LU with partial pivoting (`[L, U, P] = lu_decompose(A)`).
///
/// `P · A == L · U` where `L` is unit-lower-triangular, `U` is
/// upper-triangular, `P` is a permutation matrix.
std::tuple<Value, Value, Value>
lu_decompose(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Combined L+U output (`LU = lu(A)` single-output form).
Value lu_combined(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief QR via Householder reflections (`[Q, R] = qr_decompose(A)`).
///
/// Full-size form (not `"econ"`). `A == Q · R`. `Q` is `m × m`
/// orthogonal, `R` is `m × n` upper-triangular.
std::tuple<Value, Value>
qr_decompose(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief R-only QR output (`R = qr(A)` single-output form).
Value qr_R_only(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Singular value decomposition (`[U, S, V] = svd_decompose(A)`).
///
/// `A = U · S · V'`. One-sided Jacobi rotations.
std::tuple<Value, Value, Value>
svd_decompose(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Singular values only (`s = svd(A)` single-output form).
Value svd_values(const Value &A, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
