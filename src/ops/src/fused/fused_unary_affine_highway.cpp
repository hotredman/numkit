// ops/src/fused/fused_unary_affine_highway.cpp
//
// SIMD kernel for fusedUnaryAffine: out[i] = f(scale*x[i] + offset), one
// streaming pass instead of the per-op affine temporary + the unary's own pass.
// Only f ∈ {sqrt, floor, ceil} — each has a SIMD form bit-identical to the
// scalar libm one (sqrt maps to vsqrtpd = correctly-rounded IEEE; floor/ceil
// are exact directed roundings), so the fused result matches `f(a.*x ± b)`
// bit-for-bit no matter where the array is split. The affine is Mul-then-Add
// (two roundings, NOT FMA) to match the affine fallback.

#include <numkit/ops/fused/fused_kernels.hpp>
#include <numkit/ops/parallel_for.hpp>

#include <cmath>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "fused/fused_unary_affine_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// One inner+unary pass. The inner is either affine (p0*x + p1) or shift-divide
// ((x - p0)/p1) per `divide` — a loop-invariant branch, so the affine path is
// unchanged. VecOp maps a vector → vector (the Highway unary); ScalarOp maps the
// tail double → double (the matching libm unary).
template <class VecOp, class ScalarOp>
void unaryInnerLoop(const double *x, double p0, double p1, bool divide,
                    double *out, std::size_t n, VecOp vop, ScalarOp sop) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto vp0 = hn::Set(d, p0), vp1 = hn::Set(d, p1);
    std::size_t i = 0;
    for (; i + L <= n; i += L) {
        const auto v = hn::LoadU(d, x + i);
        const auto inner = divide ? hn::Div(hn::Sub(v, vp0), vp1)
                                  : hn::Add(hn::Mul(v, vp0), vp1);
        hn::StoreU(vop(inner), d, out + i);
    }
    for (; i < n; ++i)
        out[i] = sop(divide ? (x[i] - p0) / p1 : p0 * x[i] + p1);
}

void UnaryAffineImpl(const double *x, double p0, double p1, bool divide,
                     UnaryAffineFn fn, double *out, std::size_t n) {
    switch (fn) {
        case UnaryAffineFn::Sqrt:
            unaryInnerLoop(x, p0, p1, divide, out, n,
                           [](auto v) { return hn::Sqrt(v); },
                           [](double v) { return std::sqrt(v); });
            break;
        case UnaryAffineFn::Floor:
            unaryInnerLoop(x, p0, p1, divide, out, n,
                           [](auto v) { return hn::Floor(v); },
                           [](double v) { return std::floor(v); });
            break;
        case UnaryAffineFn::Ceil:
            unaryInnerLoop(x, p0, p1, divide, out, n,
                           [](auto v) { return hn::Ceil(v); },
                           [](double v) { return std::ceil(v); });
            break;
        case UnaryAffineFn::Fix:
            unaryInnerLoop(x, p0, p1, divide, out, n,
                           [](auto v) { return hn::Trunc(v); },
                           [](double v) { return std::trunc(v); });
            break;
        case UnaryAffineFn::Round:
            // MATLAB round = half-away-from-zero: Trunc(v + CopySign(0.5, v)),
            // mirroring numkit's RoundLoop (NOT hn::Round = round-to-even).
            unaryInnerLoop(x, p0, p1, divide, out, n,
                           [](auto v) {
                               const hn::DFromV<decltype(v)> d;
                               return hn::Trunc(hn::Add(v, hn::CopySign(hn::Set(d, 0.5), v)));
                           },
                           [](double v) { return std::round(v); });
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
        HWY_DYNAMIC_DISPATCH(UnaryAffineImpl)(x + s, scale, offset, /*divide=*/false,
                                              fn, out + s, e - s);
    });
}

void fusedUnaryShiftDiv(const double *x, double sub, double div,
                        UnaryAffineFn fn, double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, std::size_t{1} << 16, [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(UnaryAffineImpl)(x + s, sub, div, /*divide=*/true,
                                              fn, out + s, e - s);
    });
}

} // namespace numkit::ops
#endif // HWY_ONCE
