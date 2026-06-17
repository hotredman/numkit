// ops/src/fused_sq_highway.cpp
//
// SIMD kernels for the square / magnitude idioms (all enabled by x.^2 == x.*x):
//   fusedSqAffine:  out[i] = (scale*x[i] + offset)^2   — `(a.*x ± b).^2`
//   fusedSqDiff:    out[i] = (x[i] - y[i])^2           — `(x - y).^2`
//   fusedSqrtSumSq: out[i] = sqrt(x[i]^2 + y[i]^2)     — `sqrt(x.^2 + y.^2)`
// Each square is a plain Mul (no FMA), so the rounding matches the per-op path
// element-for-element. sqrt maps to vsqrtpd (correctly-rounded), and a sum of
// real squares is never negative, so fusedSqrtSumSq needs no complex/domain
// handling. NaN/Inf propagate through the plain IEEE ops as per-op.

#include <numkit/ops/fused_kernels.hpp>
#include <numkit/ops/parallel_for.hpp>

#include <cmath>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "fused_sq_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void SqAffineImpl(const double *x, double scale, double offset,
                  double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto vs = hn::Set(d, scale), vt = hn::Set(d, offset);
    std::size_t i = 0;
    for (; i + L <= n; i += L) {
        const auto v = hn::Add(hn::Mul(hn::LoadU(d, x + i), vs), vt);
        hn::StoreU(hn::Mul(v, v), d, out + i);
    }
    for (; i < n; ++i) {
        const double v = scale * x[i] + offset;
        out[i] = v * v;
    }
}

void SqDiffImpl(const double *x, const double *y, double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + L <= n; i += L) {
        const auto v = hn::Sub(hn::LoadU(d, x + i), hn::LoadU(d, y + i));
        hn::StoreU(hn::Mul(v, v), d, out + i);
    }
    for (; i < n; ++i) {
        const double v = x[i] - y[i];
        out[i] = v * v;
    }
}

void SqrtSumSqImpl(const double *x, const double *y, double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + L <= n; i += L) {
        const auto vx = hn::LoadU(d, x + i), vy = hn::LoadU(d, y + i);
        hn::StoreU(hn::Sqrt(hn::Add(hn::Mul(vx, vx), hn::Mul(vy, vy))), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::sqrt(x[i] * x[i] + y[i] * y[i]);
}

} // namespace HWY_NAMESPACE
} // namespace numkit::ops
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::ops {

HWY_EXPORT(SqAffineImpl);
HWY_EXPORT(SqDiffImpl);
HWY_EXPORT(SqrtSumSqImpl);

void fusedSqAffine(const double *x, double scale, double offset,
                   double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(SqAffineImpl)(x + s, scale, offset, out + s, e - s);
    });
}

void fusedSqDiff(const double *x, const double *y, double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(SqDiffImpl)(x + s, y + s, out + s, e - s);
    });
}

void fusedSqrtSumSq(const double *x, const double *y, double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(SqrtSumSqImpl)(x + s, y + s, out + s, e - s);
    });
}

} // namespace numkit::ops
#endif // HWY_ONCE
