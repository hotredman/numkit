// ops/src/fused/fused_affine_highway.cpp
//
// SIMD kernel for fusedAffine: out[i] = scale*x[i] + offset. NaN-preserving —
// there is no clamp, so a NaN input flows straight through (scale*NaN+offset =
// NaN), which the affine-clamp kernel cannot do (its ±inf clamp would turn a
// NaN into ±inf). Mul-then-Add (two roundings, NOT MulAdd/FMA) so the result
// is bit-identical to the per-op `a.*x + b` fallback (which also rounds twice).

#include <numkit/ops/fused/fused_kernels.hpp>
#include <numkit/ops/parallel_for.hpp>

#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "fused/fused_affine_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void AffineImpl(const double *x, double scale, double offset,
                double *out, std::size_t n) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto vs = hn::Set(d, scale), vt = hn::Set(d, offset);
    std::size_t i = 0;
    for (; i + L <= n; i += L) {
        // Mul then Add (two roundings) — NOT MulAdd/FMA — to match the per-op
        // `a.*x + b` fallback bit-for-bit.
        const auto v = hn::Add(hn::Mul(hn::LoadU(d, x + i), vs), vt);
        hn::StoreU(v, d, out + i);
    }
    for (; i < n; ++i) out[i] = scale * x[i] + offset;
}

} // namespace HWY_NAMESPACE
} // namespace numkit::ops
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::ops {

HWY_EXPORT(AffineImpl);

void fusedAffine(const double *x, double scale, double offset,
                 double *out, std::size_t n) {
    if (n == 0) return;
    // Each chunk is a disjoint output slice; no-op (sequential) unless
    // NUMKIT_WITH_THREADS and n is large.
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(AffineImpl)(x + s, scale, offset, out + s, e - s);
    });
}

} // namespace numkit::ops
#endif // HWY_ONCE
