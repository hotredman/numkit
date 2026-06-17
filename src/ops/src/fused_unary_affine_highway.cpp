// ops/src/fused_unary_affine_highway.cpp
//
// SIMD kernel for fusedUnaryAffine: out[i] = f(scale*x[i] + offset), one
// streaming pass instead of the per-op affine temporary + the unary's own pass.
// Only f ∈ {sqrt, floor, ceil} — each has a SIMD form bit-identical to the
// scalar libm one (sqrt maps to vsqrtpd = correctly-rounded IEEE; floor/ceil
// are exact directed roundings), so the fused result matches `f(a.*x ± b)`
// bit-for-bit no matter where the array is split. The affine is Mul-then-Add
// (two roundings, NOT FMA) to match the affine fallback.

#include <numkit/ops/fused_kernels.hpp>
#include <numkit/ops/parallel_for.hpp>

#include <cmath>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "fused_unary_affine_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// One affine+unary pass. VecOp maps a vector → vector (the Highway unary);
// ScalarOp maps the tail double → double (the matching libm unary).
template <class VecOp, class ScalarOp>
void unaryAffineLoop(const double *x, double scale, double offset, double *out,
                     std::size_t n, VecOp vop, ScalarOp sop) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto vs = hn::Set(d, scale), vt = hn::Set(d, offset);
    std::size_t i = 0;
    for (; i + L <= n; i += L)
        hn::StoreU(vop(hn::Add(hn::Mul(hn::LoadU(d, x + i), vs), vt)), d, out + i);
    for (; i < n; ++i) out[i] = sop(scale * x[i] + offset);
}

void UnaryAffineImpl(const double *x, double scale, double offset,
                     UnaryAffineFn fn, double *out, std::size_t n) {
    switch (fn) {
        case UnaryAffineFn::Sqrt:
            unaryAffineLoop(x, scale, offset, out, n,
                            [](auto v) { return hn::Sqrt(v); },
                            [](double v) { return std::sqrt(v); });
            break;
        case UnaryAffineFn::Floor:
            unaryAffineLoop(x, scale, offset, out, n,
                            [](auto v) { return hn::Floor(v); },
                            [](double v) { return std::floor(v); });
            break;
        case UnaryAffineFn::Ceil:
            unaryAffineLoop(x, scale, offset, out, n,
                            [](auto v) { return hn::Ceil(v); },
                            [](double v) { return std::ceil(v); });
            break;
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::ops
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::ops {

HWY_EXPORT(UnaryAffineImpl);

void fusedUnaryAffine(const double *x, double scale, double offset,
                      UnaryAffineFn fn, double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(UnaryAffineImpl)(x + s, scale, offset, fn, out + s,
                                              e - s);
    });
}

} // namespace numkit::ops
#endif // HWY_ONCE
