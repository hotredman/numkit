// ops/src/fused/fused_axpby_highway.cpp
//
// SIMD kernel for fusedAxpby: out[i] = a*x[i] + b*y[i] (two arrays, scalar
// weights). One streaming pass reads x and y once and writes out once, instead
// of the per-op path's two products + a temporary. Mul, Mul, Add (each rounds;
// NOT FMA) so the result is bit-identical to the per-op `a.*x + b.*y`.

#include <numkit/ops/fused/fused_kernels.hpp>
#include <numkit/ops/parallel_for.hpp>

#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "fused/fused_axpby_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void AxpbyImpl(const double *x, double a, const double *y, double b,
               double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto va = hn::Set(d, a), vb = hn::Set(d, b);
    std::size_t i = 0;
    for (; i + L <= n; i += L) {
        // (a*x) + (b*y), three roundings — NOT FMA — to match the per-op
        // `a.*x + b.*y` fallback bit-for-bit.
        const auto px = hn::Mul(hn::LoadU(d, x + i), va);
        const auto py = hn::Mul(hn::LoadU(d, y + i), vb);
        hn::StoreU(hn::Add(px, py), d, out + i);
    }
    for (; i < n; ++i) out[i] = a * x[i] + b * y[i];
}

} // namespace HWY_NAMESPACE
} // namespace numkit::ops
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::ops {

HWY_EXPORT(AxpbyImpl);

void fusedAxpby(const double *x, double a, const double *y, double b,
                double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(AxpbyImpl)(x + s, a, y + s, b, out + s, e - s);
    });
}

} // namespace numkit::ops
#endif // HWY_ONCE
