#include <numkit/ops/blas2.hpp>

#include <cmath>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "blas/blas2_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

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

HWY_EXPORT(GemvDoubleKernel);
HWY_EXPORT(GerDoubleKernel);

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

} // namespace numkit::ops
#endif
