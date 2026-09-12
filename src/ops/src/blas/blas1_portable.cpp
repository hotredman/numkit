#include <nk/ops/blas1.hpp>
#include <nk/ops/parallel_for.hpp>

namespace nk::ops {

void axpy(std::size_t n, double alpha, const double *x, double *y)
{
    if (alpha == 0.0 || n == 0) return;
    
    // We can use a simple threshold for parallel_for
    constexpr std::size_t kParallelThreshold = 256 * 1024;
    
    nk::detail::parallel_for(n, kParallelThreshold, [=](std::size_t start, std::size_t end) {
        for (std::size_t i = start; i < end; ++i) {
            y[i] += alpha * x[i];
        }
    });
}

} // namespace nk::ops