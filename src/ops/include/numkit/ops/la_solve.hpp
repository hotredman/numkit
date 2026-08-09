// ops/include/numkit/ops/la_solve.hpp
//
// Internal linear-solve kernel: solve `A · X = B` (LU for square A,
// Householder QR for tall A). An L0.5 ops primitive — raw column-major
// double buffers + a memory_resource, no Value, no engine. It backs both the
// `\` operator in toolboxes/builtin (mldivide / mrdivide) AND the user-facing
// wrappers (inv / linsolve / pageinv / eig …) in toolboxes/linalg.
//
// It used to live in toolboxes/builtin/include/numkit/builtin/internal/ precisely
// because "builtin must not include linalg" left nowhere shared for it. ops/
// (below both builtin and linalg) is its proper home; builtin's internal
// header now re-exports it for back-compat.
//
// Wide systems (m < n) are NOT supported here — MATLAB's mldivide returns the
// minimum-norm solution for wide matrices (needs SVD or QR-of-A'); deferred.
// All scratch goes through std::pmr::memory_resource (HARD RULE).

#pragma once

#include <complex>
#include <cstddef>
#include <memory_resource>

namespace numkit::ops {

// Solve A · X = B for X.
//   A : m×n column-major (m >= n required; wide systems return false)
//   B : m×nrhs column-major
//   X : n×nrhs column-major (caller-allocated)
// Returns false on: m < n (unsupported), singular A (square, zero pivot),
// or rank-deficient A (tall, zero column norm or zero R diagonal). Internal
// scratch is allocated via the supplied memory_resource.
bool la_solve(const double *A, std::size_t m, std::size_t n,
              const double *B, std::size_t nrhs,
              double *X,
              std::pmr::memory_resource *mr);

bool la_solve(const std::complex<double> *A, std::size_t m, std::size_t n,
              const std::complex<double> *B, std::size_t nrhs,
              std::complex<double> *X,
              std::pmr::memory_resource *mr);

bool lu_factor_inplace(double *A, std::size_t lda, std::int32_t *piv, std::size_t m, std::size_t n);
bool lu_factor_inplace(std::complex<double> *A, std::size_t lda, std::int32_t *piv, std::size_t m, std::size_t n);

} // namespace numkit::ops
