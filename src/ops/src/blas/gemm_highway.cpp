// ops/src/blas/gemm_highway.cpp
//
// High-performance column-major Highway SIMD BLAS kernels (gemm, gemv, ger, trsm).

#include <numkit/ops/blas.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "blas/gemm_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void GemmDoubleKernel(std::size_t m, std::size_t n, std::size_t k,
                      double alpha, const double *A, std::size_t lda,
                      const double *B, std::size_t ldb,
                      double beta, double *C, std::size_t ldc)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);

    for (std::size_t j = 0; j < n; ++j) {
        double *cj = C + j * ldc;
        if (beta == 0.0) {
            std::fill(cj, cj + m, 0.0);
        } else if (beta != 1.0) {
            std::size_t i = 0;
            auto v_beta = hn::Set(d, beta);
            for (; i + N <= m; i += N) {
                hn::StoreU(hn::Mul(hn::LoadU(d, cj + i), v_beta), d, cj + i);
            }
            for (; i < m; ++i) cj[i] *= beta;
        }
    }

    if (k == 0 || alpha == 0.0) return;

    for (std::size_t j = 0; j < n; ++j) {
        double *cj = C + j * ldc;
        for (std::size_t l = 0; l < k; ++l) {
            const double blj = alpha * B[l + j * ldb];
            if (blj == 0.0) continue;
            const double *al = A + l * lda;
            auto v_b = hn::Set(d, blj);

            std::size_t i = 0;
            for (; i + 4 * N <= m; i += 4 * N) {
                auto c0 = hn::MulAdd(hn::LoadU(d, al + i + 0 * N), v_b, hn::LoadU(d, cj + i + 0 * N));
                auto c1 = hn::MulAdd(hn::LoadU(d, al + i + 1 * N), v_b, hn::LoadU(d, cj + i + 1 * N));
                auto c2 = hn::MulAdd(hn::LoadU(d, al + i + 2 * N), v_b, hn::LoadU(d, cj + i + 2 * N));
                auto c3 = hn::MulAdd(hn::LoadU(d, al + i + 3 * N), v_b, hn::LoadU(d, cj + i + 3 * N));

                hn::StoreU(c0, d, cj + i + 0 * N);
                hn::StoreU(c1, d, cj + i + 1 * N);
                hn::StoreU(c2, d, cj + i + 2 * N);
                hn::StoreU(c3, d, cj + i + 3 * N);
            }
            for (; i + N <= m; i += N) {
                auto c0 = hn::MulAdd(hn::LoadU(d, al + i), v_b, hn::LoadU(d, cj + i));
                hn::StoreU(c0, d, cj + i);
            }
            for (; i < m; ++i) {
                cj[i] += al[i] * blj;
            }
        }
    }
}

void GemvDoubleKernel(std::size_t m, std::size_t n,
                      double alpha, const double *A, std::size_t lda,
                      const double *x, std::size_t incx,
                      double beta, double *y, std::size_t incy)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);

    if (beta == 0.0) {
        for (std::size_t i = 0; i < m; ++i) y[i * incy] = 0.0;
    } else if (beta != 1.0) {
        for (std::size_t i = 0; i < m; ++i) y[i * incy] *= beta;
    }

    if (alpha == 0.0 || n == 0) return;

    if (incy == 1) {
        for (std::size_t j = 0; j < n; ++j) {
            const double xj = alpha * x[j * incx];
            if (xj == 0.0) continue;
            const double *aj = A + j * lda;
            auto v_x = hn::Set(d, xj);

            std::size_t i = 0;
            for (; i + N <= m; i += N) {
                auto v_y = hn::MulAdd(hn::LoadU(d, aj + i), v_x, hn::LoadU(d, y + i));
                hn::StoreU(v_y, d, y + i);
            }
            for (; i < m; ++i) y[i] += aj[i] * xj;
        }
    } else {
        for (std::size_t j = 0; j < n; ++j) {
            const double xj = alpha * x[j * incx];
            if (xj == 0.0) continue;
            const double *aj = A + j * lda;
            for (std::size_t i = 0; i < m; ++i) y[i * incy] += aj[i] * xj;
        }
    }
}

void GerDoubleKernel(std::size_t m, std::size_t n,
                     double alpha, const double *x, std::size_t incx,
                     const double *y, std::size_t incy,
                     double *A, std::size_t lda)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);

    if (alpha == 0.0) return;

    for (std::size_t j = 0; j < n; ++j) {
        const double yj = alpha * y[j * incy];
        if (yj == 0.0) continue;
        double *aj = A + j * lda;
        auto v_y = hn::Set(d, yj);

        if (incx == 1) {
            std::size_t i = 0;
            for (; i + N <= m; i += N) {
                auto v_a = hn::MulAdd(hn::LoadU(d, x + i), v_y, hn::LoadU(d, aj + i));
                hn::StoreU(v_a, d, aj + i);
            }
            for (; i < m; ++i) aj[i] += x[i] * yj;
        } else {
            for (std::size_t i = 0; i < m; ++i) aj[i] += x[i * incx] * yj;
        }
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::ops
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::ops {

HWY_EXPORT(GemmDoubleKernel);
HWY_EXPORT(GemvDoubleKernel);
HWY_EXPORT(GerDoubleKernel);

void gemm(std::size_t m, std::size_t n, std::size_t k,
          double alpha, const double *A, std::size_t lda,
          const double *B, std::size_t ldb,
          double beta, double *C, std::size_t ldc)
{
    HWY_DYNAMIC_DISPATCH(GemmDoubleKernel)(m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
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

    // SoA split-complex vectorization path
    std::vector<double> Ar(m), Ai(m);
    for (std::size_t j = 0; j < n; ++j) {
        Complex *cj = C + j * ldc;
        for (std::size_t l = 0; l < k; ++l) {
            Complex blj = alpha * B[l + j * ldb];
            if (blj == Complex(0.0, 0.0)) continue;
            const Complex *al = A + l * lda;

            for (std::size_t i = 0; i < m; ++i) {
                Ar[i] = al[i].real();
                Ai[i] = al[i].imag();
            }

            double br = blj.real();
            double bi = blj.imag();

            for (std::size_t i = 0; i < m; ++i) {
                double cr = Ar[i] * br - Ai[i] * bi;
                double ci = Ar[i] * bi + Ai[i] * br;
                cj[i] += Complex(cr, ci);
            }
        }
    }
}

void gemv(std::size_t m, std::size_t n,
          double alpha, const double *A, std::size_t lda,
          const double *x, std::size_t incx,
          double beta, double *y, std::size_t incy)
{
    HWY_DYNAMIC_DISPATCH(GemvDoubleKernel)(m, n, alpha, A, lda, x, incx, beta, y, incy);
}

void ger(std::size_t m, std::size_t n,
         double alpha, const double *x, std::size_t incx,
         const double *y, std::size_t incy,
         double *A, std::size_t lda)
{
    HWY_DYNAMIC_DISPATCH(GerDoubleKernel)(m, n, alpha, x, incx, y, incy, A, lda);
}

void trsm(MatrixSide side, MatrixUplo uplo, MatrixTranspose trans, MatrixDiag diag,
          std::size_t m, std::size_t n,
          double alpha, const double *A, std::size_t lda,
          double *B, std::size_t ldb)
{
    if (m == 0 || n == 0) return;

    if (alpha != 1.0) {
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < m; ++i) {
                B[i + j * ldb] *= alpha;
            }
        }
    }

    if (side == MatrixSide::Left) {
        if (uplo == MatrixUplo::Lower && trans == MatrixTranspose::NoTrans) {
            // L * X = B (Lower triangular left solve)
            for (std::size_t j = 0; j < n; ++j) {
                for (std::size_t k = 0; k < m; ++k) {
                    if (diag == MatrixDiag::NonUnit) {
                        B[k + j * ldb] /= A[k + k * lda];
                    }
                    const double xkj = B[k + j * ldb];
                    if (xkj == 0.0) continue;
                    for (std::size_t i = k + 1; i < m; ++i) {
                        B[i + j * ldb] -= A[i + k * lda] * xkj;
                    }
                }
            }
        } else if (uplo == MatrixUplo::Upper && trans == MatrixTranspose::NoTrans) {
            // U * X = B (Upper triangular left solve)
            for (std::size_t j = 0; j < n; ++j) {
                for (std::intptr_t k = static_cast<std::intptr_t>(m) - 1; k >= 0; --k) {
                    if (diag == MatrixDiag::NonUnit) {
                        B[k + j * ldb] /= A[k + k * lda];
                    }
                    const double xkj = B[k + j * ldb];
                    if (xkj == 0.0) continue;
                    for (std::intptr_t i = k - 1; i >= 0; --i) {
                        B[i + j * ldb] -= A[i + k * lda] * xkj;
                    }
                }
            }
        }
    } else {
        // Right solve: X * A = B
        if (uplo == MatrixUplo::Upper && trans == MatrixTranspose::NoTrans) {
            for (std::size_t j = 0; j < n; ++j) {
                if (diag == MatrixDiag::NonUnit) {
                    for (std::size_t i = 0; i < m; ++i) B[i + j * ldb] /= A[j + j * lda];
                }
                for (std::size_t k = j + 1; k < n; ++k) {
                    const double akj = A[j + k * lda];
                    for (std::size_t i = 0; i < m; ++i) {
                        B[i + k * ldb] -= B[i + j * ldb] * akj;
                    }
                }
            }
        }
    }
}

} // namespace numkit::ops
#endif
