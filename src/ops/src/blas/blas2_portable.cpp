#include <numkit/ops/blas2.hpp>
#include <numkit/ops/parallel_for.hpp>

namespace numkit::ops {

void gemv(std::size_t m, std::size_t n,
          double alpha, const double *A, std::size_t lda,
          const double *x, std::size_t incx,
          double beta, double *y, std::size_t incy)
{
    if (m == 0) return;
    
    // Scale y by beta
    if (beta == 0.0) {
        for (std::size_t i = 0; i < m; ++i) y[i * incy] = 0.0;
    } else if (beta != 1.0) {
        for (std::size_t i = 0; i < m; ++i) y[i * incy] *= beta;
    }

    if (alpha == 0.0 || n == 0) return;

    // A is m x n (column-major)
    // We parallelize over columns to avoid data races on y
    constexpr std::size_t kParallelThreshold = 1024;
    numkit::detail::parallel_for(n, kParallelThreshold, [=](std::size_t j_start, std::size_t j_end) {
        // If we parallelize over columns (n), multiple threads will write to y.
        // Wait, parallel_for over n would cause data races on y!
        // We MUST parallelize over m (rows) to avoid races on y.
        // Let's not use parallel_for for GEMV in this simple portable version, 
        // or we parallelize over m and inner loop over n.
    });
    // Actually, parallelizing GEMV over rows (m) is safer:
    
    constexpr std::size_t kRowThreshold = 1024;
    numkit::detail::parallel_for(m, kRowThreshold, [=](std::size_t i_start, std::size_t i_end) {
        for (std::size_t j = 0; j < n; ++j) {
            const double xj = alpha * x[j * incx];
            if (xj == 0.0) continue;
            const double *aj = A + j * lda;
            for (std::size_t i = i_start; i < i_end; ++i) {
                y[i * incy] += aj[i] * xj;
            }
        }
    });
}

void ger(std::size_t m, std::size_t n,
         double alpha, const double *x, std::size_t incx,
         const double *y, std::size_t incy,
         double *A, std::size_t lda)
{
    if (alpha == 0.0 || m == 0 || n == 0) return;

    constexpr std::size_t kParallelThreshold = 256;
    numkit::detail::parallel_for(n, kParallelThreshold, [=](std::size_t j_start, std::size_t j_end) {
        for (std::size_t j = j_start; j < j_end; ++j) {
            const double yj = alpha * y[j * incy];
            if (yj == 0.0) continue;
            double *aj = A + j * lda;
            if (incx == 1) {
                for (std::size_t i = 0; i < m; ++i) aj[i] += x[i] * yj;
            } else {
                for (std::size_t i = 0; i < m; ++i) aj[i] += x[i * incx] * yj;
            }
        }
    });
}

} // namespace numkit::ops