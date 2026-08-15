// ops/include/numkit/ops/blas3.hpp
#pragma once

#include <cstddef>
#include <complex>

namespace numkit::ops {

enum class MatrixSide { Left, Right };
enum class MatrixUplo { Upper, Lower };
enum class MatrixTranspose { NoTrans, Trans, ConjTrans };
enum class MatrixDiag { NonUnit, Unit };

/// @brief General matrix-matrix multiply C = alpha * op(A) * op(B) + beta * C (real double).
/// Column-major matrices: op(A) is (m x k), op(B) is (k x n), C is (m x n).
void gemm(MatrixTranspose transa, MatrixTranspose transb,
          std::size_t m, std::size_t n, std::size_t k,
          double alpha, const double *A, std::size_t lda,
          const double *B, std::size_t ldb,
          double beta, double *C, std::size_t ldc);

/// @brief General matrix-matrix multiply C = alpha * op(A) * op(B) + beta * C (complex double).
void gemm(MatrixTranspose transa, MatrixTranspose transb,
          std::size_t m, std::size_t n, std::size_t k,
          std::complex<double> alpha, const std::complex<double> *A, std::size_t lda,
          const std::complex<double> *B, std::size_t ldb,
          std::complex<double> beta, std::complex<double> *C, std::size_t ldc);

/// @brief Triangular matrix solve (real double).
void trsm(MatrixSide side, MatrixUplo uplo, MatrixTranspose trans, MatrixDiag diag,
          std::size_t m, std::size_t n,
          double alpha, const double *A, std::size_t lda,
          double *B, std::size_t ldb);

/// @brief Triangular matrix solve (complex double).
void trsm(MatrixSide side, MatrixUplo uplo, MatrixTranspose trans, MatrixDiag diag,
          std::size_t m, std::size_t n,
          std::complex<double> alpha, const std::complex<double> *A, std::size_t lda,
          std::complex<double> *B, std::size_t ldb);

/// @brief Symmetric rank-k update C = alpha * A * A^T + beta * C (real double).
void syrk(MatrixUplo uplo, MatrixTranspose trans,
          std::size_t n, std::size_t k,
          double alpha, const double *A, std::size_t lda,
          double beta, double *C, std::size_t ldc);

/// @brief Hermitian/Symmetric rank-k update C = alpha * A * A^H + beta * C (complex double).
void syrk(MatrixUplo uplo, MatrixTranspose trans,
          std::size_t n, std::size_t k,
          std::complex<double> alpha, const std::complex<double> *A, std::size_t lda,
          std::complex<double> beta, std::complex<double> *C, std::size_t ldc);

} // namespace numkit::ops
