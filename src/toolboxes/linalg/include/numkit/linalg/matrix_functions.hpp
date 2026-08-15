// toolboxes/linalg/include/numkit/linalg/matrix_functions.hpp
//
// Matrix functions: expm, logm, sqrtm. Migrated from
// toolboxes/builtin/src/language/arrays/matrix.cpp.

#pragma once

#include <complex>
#include <functional>
#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Matrix exponential (`B = expm(A)`).
///
/// Padé(6) approximation with scaling-and-squaring (Higham 2005).
Value expm(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix logarithm for symmetric positive-definite A.
Value logm_sym(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix square root for symmetric PSD A.
Value sqrtm_sym(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix square root for general square matrix A (`R = sqrtm(A)`).
/// Via Björck–Hammarling recurrence on Schur form.
Value sqrtm(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix logarithm for general square matrix A (`L = logm(A)`).
/// Via Schur decomposition and inverse scaling-and-squaring.
Value logm(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Solve Sylvester equation `A · X + X · B = C` (`X = sylvester(A, B, C)`).
/// Via Bartels–Stewart algorithm on Schur forms.
Value sylvester(const Value &A, const Value &B, const Value &C,
                std::pmr::memory_resource *mr = nullptr);

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

/// @brief General matrix function evaluator `funm(A, fun)`.
/// Evaluates scalar function f on square matrix A via Schur–Parlett algorithm.
Value funm(const Value &A, std::function<std::complex<double>(std::complex<double>)> f,
           std::pmr::memory_resource *mr = nullptr);

/// @brief General matrix function evaluator by string name (`'exp'`, `'sin'`, `'cos'`, `'sinh'`, `'cosh'`).
Value funm(const Value &A, const std::string &fnName,
           std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
