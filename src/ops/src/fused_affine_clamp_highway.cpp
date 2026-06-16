// ops/src/fused_affine_clamp_highway.cpp
//
// SIMD kernel for fusedAffineClamp: out[i] = max(lo, min(hi, scale*x[i]+offset)).
// Fixed structure → intermediates stay in registers (no micro-VM spill), one
// FMA + a clamp per element, single streaming pass. min/max replicate
// std::fmin/fmax NaN semantics so the result matches the per-op fallback.

#include <numkit/ops/fused_kernels.hpp>
#include <numkit/ops/parallel_for.hpp>

#include <cmath>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "fused_affine_clamp_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// std::fmin/fmax NaN semantics (a NaN operand is ignored; both NaN → NaN).
template <class V> HWY_INLINE V FMin(V a, V b) {
    return hn::IfThenElse(hn::IsNaN(a), b, hn::IfThenElse(hn::IsNaN(b), a, hn::Min(a, b)));
}
template <class V> HWY_INLINE V FMax(V a, V b) {
    return hn::IfThenElse(hn::IsNaN(a), b, hn::IfThenElse(hn::IsNaN(b), a, hn::Max(a, b)));
}

void AffineClampImpl(const double *x, double scale, double offset,
                     double lo, double hi, double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto vs = hn::Set(d, scale), vt = hn::Set(d, offset);
    const auto vlo = hn::Set(d, lo), vhi = hn::Set(d, hi);
    std::size_t i = 0;
    for (; i + L <= n; i += L) {
        // Mul then Add (two roundings) — NOT MulAdd/FMA — so the result is
        // bit-identical to the per-op `a.*x + b` fallback (which rounds twice).
        auto v = hn::Add(hn::Mul(hn::LoadU(d, x + i), vs), vt);
        v = FMax(vlo, FMin(vhi, v));
        hn::StoreU(v, d, out + i);
    }
    for (; i < n; ++i) {
        const double v = scale * x[i] + offset;
        out[i] = std::fmax(lo, std::fmin(hi, v));
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::ops
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::ops {

HWY_EXPORT(AffineClampImpl);

void fusedAffineClamp(const double *x, double scale, double offset,
                      double lo, double hi, double *out, std::size_t n) {
    if (n == 0) return;
    // Each chunk is a disjoint output slice; no-op (sequential) unless
    // NUMKIT_WITH_THREADS and n is large.
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(AffineClampImpl)(x + s, scale, offset, lo, hi,
                                              out + s, e - s);
    });
}

} // namespace numkit::ops
#endif // HWY_ONCE
