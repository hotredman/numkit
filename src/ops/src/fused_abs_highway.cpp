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

// inner is affine (p0*x + p1) or shift-divide ((x - p0)/p1) per `divide` (a
// loop-invariant branch). abs is exact (a sign-bit clear), so each form is
// bit-identical to its per-op spelling.
void AbsAffineImpl(const double *x, double p0, double p1, bool divide,
                   double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto vp0 = hn::Set(d, p0), vp1 = hn::Set(d, p1);
    std::size_t i = 0;
    for (; i + L <= n; i += L) {
        const auto xv = hn::LoadU(d, x + i);
        const auto v = divide ? hn::Div(hn::Sub(xv, vp0), vp1)
                              : hn::Add(hn::Mul(xv, vp0), vp1);
        hn::StoreU(hn::Abs(v), d, out + i);
    }
    for (; i < n; ++i)
        out[i] = std::fabs(divide ? (x[i] - p0) / p1 : p0 * x[i] + p1);
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
        HWY_DYNAMIC_DISPATCH(AbsAffineImpl)(x + s, scale, offset, /*divide=*/false,
                                            out + s, e - s);
    });
}

void fusedAbsShiftDiv(const double *x, double sub, double div,
                      double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(AbsAffineImpl)(x + s, sub, div, /*divide=*/true,
                                            out + s, e - s);
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
