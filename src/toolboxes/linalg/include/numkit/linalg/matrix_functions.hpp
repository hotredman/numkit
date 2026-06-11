// toolboxes/linalg/include/numkit/linalg/matrix_functions.hpp
//
// Matrix functions: expm, logm, sqrtm. Migrated from
// toolboxes/builtin/src/language/arrays/matrix.cpp.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Matrix exponential (`B = expm(A)`).
///
/// Padé(6) approximation with scaling-and-squaring (Higham 2005).
Value expm(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix logarithm for symmetric positive-definite A.
///
/// Via eigendecomposition: `logm(A) = V · diag(log(eig)) · V'`.
/// General logm requires complex Schur (deferred).
Value logm_sym(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix square root for symmetric PSD A.
Value sqrtm_sym(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Action of matrix exponential on a vector (`w = expmv(t, A, v)`).
///
/// Computes `w ≈ exp(t · A) · v` without forming the full matrix
/// exponential. Krylov subspace via Arnoldi + small expm on the
/// resulting upper Hessenberg matrix; fixed Krylov dimension
/// `m = min(30, n)` in v1 (adaptive refinement deferred).
///
/// Faster than `expm(t*A) * v` for large `n` when v is a single
/// vector; identical to it at machine precision for small n.
Value expmv(double t, const Value &A, const Value &v,
            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
