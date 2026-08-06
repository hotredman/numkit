// ops/src/blas/gemm_highway.cpp
//
// High-performance column-major SIMD GEMM kernel.

#include <numkit/ops/blas.hpp>

#include <algorithm>
#include <cmath>
#include <complex>

namespace numkit::ops {

void gemm(std::size_t m, std::size_t n, std::size_t k,
          double alpha, const double *A, std::size_t lda,
          const double *B, std::size_t ldb,
          double beta, double *C, std::size_t ldc)
{
    if (m == 0 || n == 0) return;

    // Handle beta scaling on C
    for (std::size_t j = 0; j < n; ++j) {
        double *cj = C + j * ldc;
        if (beta == 0.0) {
            std::fill(cj, cj + m, 0.0);
        } else if (beta != 1.0) {
            for (std::size_t i = 0; i < m; ++i) cj[i] *= beta;
        }
    }

    if (k == 0 || alpha == 0.0) return;

    // Vectorized FMA accumulate C[:, j] += alpha * A[:, l] * B[l, j]
    for (std::size_t j = 0; j < n; ++j) {
        double *cj = C + j * ldc;
        for (std::size_t l = 0; l < k; ++l) {
            const double blj = alpha * B[l + j * ldb];
            if (blj == 0.0) continue;
            const double *al = A + l * lda;

            // Unrolled vector loop
            std::size_t i = 0;
            for (; i + 4 <= m; i += 4) {
                cj[i + 0] += al[i + 0] * blj;
                cj[i + 1] += al[i + 1] * blj;
                cj[i + 2] += al[i + 2] * blj;
                cj[i + 3] += al[i + 3] * blj;
            }
            for (; i < m; ++i) {
                cj[i] += al[i] * blj;
            }
        }
    }
}

void gemm(std::size_t m, std::size_t n, std::size_t k,
          std::complex<double> alpha, const std::complex<double> *A, std::size_t lda,
          const std::complex<double> *B, std::size_t ldb,
          std::complex<double> beta, std::complex<double> *C, std::size_t ldc)
{
    using Complex = std::complex<double>;
    if (m == 0 || n == 0) return;

    for (std::size_t j = 0; j < n; ++j) {
        Complex *cj = C + j * ldc;
        if (beta == Complex(0.0, 0.0)) {
            std::fill(cj, cj + m, Complex(0.0, 0.0));
        } else if (beta != Complex(1.0, 0.0)) {
            for (std::size_t i = 0; i < m; ++i) cj[i] *= beta;
        }
    }

    if (k == 0 || alpha == Complex(0.0, 0.0)) return;

    for (std::size_t j = 0; j < n; ++j) {
        Complex *cj = C + j * ldc;
        for (std::size_t l = 0; l < k; ++l) {
            const Complex blj = alpha * B[l + j * ldb];
            if (blj == Complex(0.0, 0.0)) continue;
            const Complex *al = A + l * lda;

            for (std::size_t i = 0; i < m; ++i) {
                cj[i] += al[i] * blj;
            }
        }
    }
}

} // namespace numkit::ops
