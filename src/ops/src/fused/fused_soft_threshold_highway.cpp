// ops/src/fused/fused_soft_threshold_highway.cpp
//
// SIMD kernel for fusedSoftThreshold: out[i] = sign(x) * max(0, |x| - t), the
// wavelet/L1 shrinkage operator. One pass instead of sign + abs + sub + max +
// mul (five per-op passes). Every step is exact/IEEE — sign is replicated from
// numkit's signOp (NaN→NaN, v>0→1, v<0→-1, else→0), abs is a sign-bit clear,
// and max(0, d) omits NaN (std::fmax semantics, matching numkit's max(0,·)) —
// so the result is bit-identical to `sign(x) .* max(0, abs(x) - t)`.

#include <numkit/ops/fused/fused_kernels.hpp>
#include <numkit/ops/parallel_for.hpp>

#include <cmath>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "fused/fused_soft_threshold_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void SoftThresholdImpl(const double *x, double t, double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto vt = hn::Set(d, t);
    const auto zero = hn::Zero(d), one = hn::Set(d, 1.0), neg1 = hn::Set(d, -1.0);
    std::size_t i = 0;
    for (; i + L <= n; i += L) {
        const auto v = hn::LoadU(d, x + i);
        // sign(v): NaN→v, v>0→1, v<0→-1, else→0 (== numkit signOp).
        auto s = hn::IfThenElse(hn::Gt(v, zero), one,
                                hn::IfThenElse(hn::Lt(v, zero), neg1, zero));
        s = hn::IfThenElse(hn::IsNaN(v), v, s);
        // max(0, |v| - t), with a NaN argument omitted (→ 0), like std::fmax.
        const auto dd = hn::Sub(hn::Abs(v), vt);
        const auto m = hn::IfThenElse(hn::IsNaN(dd), zero, hn::Max(zero, dd));
        hn::StoreU(hn::Mul(s, m), d, out + i);
    }
    for (; i < n; ++i) {
        const double v = x[i];
        const double s = std::isnan(v) ? v : (v > 0 ? 1.0 : (v < 0 ? -1.0 : 0.0));
        out[i] = s * std::fmax(0.0, std::fabs(v) - t);
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::ops
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::ops {

HWY_EXPORT(SoftThresholdImpl);

void fusedSoftThreshold(const double *x, double t, double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(SoftThresholdImpl)(x + s, t, out + s, e - s);
    });
}

} // namespace numkit::ops
#endif // HWY_ONCE
