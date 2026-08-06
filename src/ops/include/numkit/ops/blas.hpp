// ops/include/numkit/ops/blas.hpp
//
// SIMD BLAS-like microkernels (gemm, trsm) in src/ops.

#pragma once

#include <cstddef>
#include <complex>

namespace numkit::ops {

/// @brief General matrix-matrix multiply C = alpha * A * B + beta * C (real double).
/// Column-major matrices: A (m x k), B (k x n), C (m x n).
void gemm(std::size_t m, std::size_t n, std::size_t k,
          double alpha, const double *A, std::size_t lda,
          const double *B, std::size_t ldb,
          double beta, double *C, std::size_t ldc);

/// @brief General matrix-matrix multiply C = alpha * A * B + beta * C (complex double).
void gemm(std::size_t m, std::size_t n, std::size_t k,
          std::complex<double> alpha, const std::complex<double> *A, std::size_t lda,
          const std::complex<double> *B, std::size_t ldb,
          std::complex<double> beta, std::complex<double> *C, std::size_t ldc);

} // namespace numkit::ops
