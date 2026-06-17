// ops/src/fused/fused_shift_scale_highway.cpp
//
// SIMD kernels for the center-then-combine idioms:
//   fusedShiftScaleMul: out[i] = (x[i] - sub) * mul   — `(x - c).*s`
//   fusedShiftScaleDiv: out[i] = (x[i] - sub) / div   — `(x - c)./d`
// One streaming pass instead of the per-op subtract + temporary + scale. Sub
// then Mul/Div (two roundings, NOT a fused step) so each is bit-identical to
// its per-op spelling. The Div variant cannot be folded into the Mul one:
// (x-c)*(1/d) would round 1/d first and diverge from (x-c)/d. NaN/Inf flow
// through the plain IEEE ops exactly as per-op (no fmin/fmax involved).

#include <numkit/ops/fused/fused_kernels.hpp>
#include <numkit/ops/parallel_for.hpp>

#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "fused/fused_shift_scale_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void ShiftScaleMulImpl(const double *x, double sub, double mul,
                       double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto vsub = hn::Set(d, sub), vmul = hn::Set(d, mul);
    std::size_t i = 0;
    for (; i + L <= n; i += L)
        hn::StoreU(hn::Mul(hn::Sub(hn::LoadU(d, x + i), vsub), vmul), d, out + i);
    for (; i < n; ++i) out[i] = (x[i] - sub) * mul;
}

void ShiftScaleDivImpl(const double *x, double sub, double div,
                       double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto vsub = hn::Set(d, sub), vdiv = hn::Set(d, div);
    std::size_t i = 0;
    for (; i + L <= n; i += L)
        hn::StoreU(hn::Div(hn::Sub(hn::LoadU(d, x + i), vsub), vdiv), d, out + i);
    for (; i < n; ++i) out[i] = (x[i] - sub) / div;
}

} // namespace HWY_NAMESPACE
} // namespace numkit::ops
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::ops {

HWY_EXPORT(ShiftScaleMulImpl);
HWY_EXPORT(ShiftScaleDivImpl);

void fusedShiftScaleMul(const double *x, double sub, double mul,
                        double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(ShiftScaleMulImpl)(x + s, sub, mul, out + s, e - s);
    });
}

void fusedShiftScaleDiv(const double *x, double sub, double div,
                        double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(ShiftScaleDivImpl)(x + s, sub, div, out + s, e - s);
    });
}

} // namespace numkit::ops
#endif // HWY_ONCE
