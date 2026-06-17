// ops/src/fused_abs_highway.cpp
//
// SIMD kernels for the absolute-value idioms:
//   fusedAbsAffine: out[i] = |scale*x[i] + offset|   — `abs(a.*x ± b)`, `abs(x-c)`
//   fusedAbsDiff:   out[i] = |x[i] - y[i]|           — `abs(x - y)` (L1 residual)
// abs is exact (a sign-bit clear, no rounding), so the only roundings are the
// Mul+Add / Sub before it — making each kernel bit-identical to its per-op
// spelling. NaN/Inf flow through naturally (|NaN| = NaN, |±Inf| = Inf).

#include <numkit/ops/fused_kernels.hpp>
#include <numkit/ops/parallel_for.hpp>

#include <cmath>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "fused_abs_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void AbsAffineImpl(const double *x, double scale, double offset,
                   double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto vs = hn::Set(d, scale), vt = hn::Set(d, offset);
    std::size_t i = 0;
    for (; i + L <= n; i += L)
        hn::StoreU(hn::Abs(hn::Add(hn::Mul(hn::LoadU(d, x + i), vs), vt)), d, out + i);
    for (; i < n; ++i) out[i] = std::fabs(scale * x[i] + offset);
}

void AbsDiffImpl(const double *x, const double *y, double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + L <= n; i += L)
        hn::StoreU(hn::Abs(hn::Sub(hn::LoadU(d, x + i), hn::LoadU(d, y + i))), d, out + i);
    for (; i < n; ++i) out[i] = std::fabs(x[i] - y[i]);
}

} // namespace HWY_NAMESPACE
} // namespace numkit::ops
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::ops {

HWY_EXPORT(AbsAffineImpl);
HWY_EXPORT(AbsDiffImpl);

void fusedAbsAffine(const double *x, double scale, double offset,
                    double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(AbsAffineImpl)(x + s, scale, offset, out + s, e - s);
    });
}

void fusedAbsDiff(const double *x, const double *y, double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(AbsDiffImpl)(x + s, y + s, out + s, e - s);
    });
}

} // namespace numkit::ops
#endif // HWY_ONCE
