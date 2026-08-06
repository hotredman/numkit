// ops/include/numkit/ops/blas.hpp
//
// SIMD BLAS-like microkernels (gemm, gemv, ger, trsm) in src/ops.

#pragma once

#include <cstddef>
#include <complex>

namespace numkit::ops {

enum class MatrixSide { Left, Right };
enum class MatrixUplo { Upper, Lower };
enum class MatrixTranspose { NoTrans, Trans, ConjTrans };
enum class MatrixDiag { NonUnit, Unit };

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

/// @brief General matrix-vector multiply y = alpha * A * x + beta * y (real double).
void gemv(std::size_t m, std::size_t n,
          double alpha, const double *A, std::size_t lda,
          const double *x, std::size_t incx,
          double beta, double *y, std::size_t incy);

/// @brief Rank-1 update A = alpha * x * y^T + A (real double).
void ger(std::size_t m, std::size_t n,
         double alpha, const double *x, std::size_t incx,
         const double *y, std::size_t incy,
         double *A, std::size_t lda);

/// @brief Triangular matrix solve (real double).
void trsm(MatrixSide side, MatrixUplo uplo, MatrixTranspose trans, MatrixDiag diag,
          std::size_t m, std::size_t n,
          double alpha, const double *A, std::size_t lda,
          double *B, std::size_t ldb);

} // namespace numkit::ops
