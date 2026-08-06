#include <numkit/ops/blas1.hpp>

#include <cmath>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "blas/blas1_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void AxpyDoubleKernel(std::size_t n, double alpha, const double *x, double *y)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    auto v_a = hn::Set(d, alpha);

    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v_y = hn::MulAdd(hn::LoadU(d, x + i), v_a, hn::LoadU(d, y + i));
        hn::StoreU(v_y, d, y + i);
    }
    for (; i < n; ++i) y[i] += x[i] * alpha;
}

} // namespace HWY_NAMESPACE
} // namespace numkit::ops
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::ops {

HWY_EXPORT(AxpyDoubleKernel);

void axpy(std::size_t n, double alpha, const double *x, double *y)
{
    HWY_DYNAMIC_DISPATCH(AxpyDoubleKernel)(n, alpha, x, y);
}

} // namespace numkit::ops
#endif
