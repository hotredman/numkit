// ops/src/fused/fused_affine_clamp_highway.cpp
//
// SIMD kernel for fusedAffineClamp: out[i] = max(lo, min(hi, scale*x[i]+offset)).
// Fixed structure → intermediates stay in registers (no micro-VM spill), one
// FMA + a clamp per element, single streaming pass. min/max replicate
// std::fmin/fmax NaN semantics so the result matches the per-op fallback.

#include <numkit/ops/fused/fused_kernels.hpp>
#include <numkit/ops/parallel_for.hpp>

#include <cmath>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "fused/fused_affine_clamp_highway.cpp"
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

// inner is affine (p0*x + p1) or shift-divide ((x - p0)/p1) per `divide` (a
// loop-invariant branch). The affine is Mul then Add (two roundings) — NOT
// MulAdd/FMA — so it is bit-identical to the per-op `a.*x + b`; the divide form
// is bit-identical to per-op `(x - c)./d`. The clamp's fmin/fmax NaN semantics
// match the per-op min/max either way.
void AffineClampImpl(const double *x, double p0, double p1, bool divide,
                     double lo, double hi, double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto vp0 = hn::Set(d, p0), vp1 = hn::Set(d, p1);
    const auto vlo = hn::Set(d, lo), vhi = hn::Set(d, hi);
    std::size_t i = 0;
    for (; i + L <= n; i += L) {
        const auto xv = hn::LoadU(d, x + i);
        auto v = divide ? hn::Div(hn::Sub(xv, vp0), vp1)
                        : hn::Add(hn::Mul(xv, vp0), vp1);
        v = FMax(vlo, FMin(vhi, v));
        hn::StoreU(v, d, out + i);
    }
    for (; i < n; ++i) {
        const double v = divide ? (x[i] - p0) / p1 : p0 * x[i] + p1;
        out[i] = std::fmax(lo, std::fmin(hi, v));
    }
}

// min-outer spelling: min(hi, max(lo, v)). Same as above for finite v; on NaN
// it yields lo (FMax(lo,NaN)=lo then FMin(hi,lo)=lo), matching the per-op
// `min(hi, max(lo, v))` nesting.
void AffineClampMinOuterImpl(const double *x, double p0, double p1, bool divide,
                             double lo, double hi, double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto vp0 = hn::Set(d, p0), vp1 = hn::Set(d, p1);
    const auto vlo = hn::Set(d, lo), vhi = hn::Set(d, hi);
    std::size_t i = 0;
    for (; i + L <= n; i += L) {
        const auto xv = hn::LoadU(d, x + i);
        auto v = divide ? hn::Div(hn::Sub(xv, vp0), vp1)
                        : hn::Add(hn::Mul(xv, vp0), vp1);
        v = FMin(vhi, FMax(vlo, v));
        hn::StoreU(v, d, out + i);
    }
    for (; i < n; ++i) {
        const double v = divide ? (x[i] - p0) / p1 : p0 * x[i] + p1;
        out[i] = std::fmin(hi, std::fmax(lo, v));
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::ops
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::ops {

HWY_EXPORT(AffineClampImpl);
HWY_EXPORT(AffineClampMinOuterImpl);

void fusedAffineClamp(const double *x, double scale, double offset,
                      double lo, double hi, double *out, std::size_t n) {
    if (n == 0) return;
    // Each chunk is a disjoint output slice; no-op (sequential) unless
    // NUMKIT_WITH_THREADS and n is large.
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(AffineClampImpl)(x + s, scale, offset, /*divide=*/false,
                                              lo, hi, out + s, e - s);
    });
}

void fusedAffineClampMinOuter(const double *x, double scale, double offset,
                              double lo, double hi, double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(AffineClampMinOuterImpl)(x + s, scale, offset,
                                                      /*divide=*/false, lo, hi,
                                                      out + s, e - s);
    });
}

void fusedAffineClampShiftDiv(const double *x, double sub, double div,
                              double lo, double hi, double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(AffineClampImpl)(x + s, sub, div, /*divide=*/true,
                                              lo, hi, out + s, e - s);
    });
}

void fusedAffineClampMinOuterShiftDiv(const double *x, double sub, double div,
                                      double lo, double hi, double *out,
                                      std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(AffineClampMinOuterImpl)(x + s, sub, div,
                                                      /*divide=*/true, lo, hi,
                                                      out + s, e - s);
    });
}

} // namespace numkit::ops
#endif // HWY_ONCE
