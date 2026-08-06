// ops/include/numkit/ops/blas2.hpp
#pragma once

#include <cstddef>

namespace numkit::ops {

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

} // namespace numkit::ops
