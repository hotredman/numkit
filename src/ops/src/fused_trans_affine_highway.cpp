// ops/src/fused_trans_affine_highway.cpp
//
// SIMD kernel for fusedTransAffine: out[i] = f(scale*x[i] + offset), f ∈
// {exp, expm1}. These transcendentals' Highway form (hwy/contrib/math, SLEEF-
// derived) differs from libm by a few ULP, so — unlike sqrt/floor/ceil — the
// kernel must MIRROR numkit's exp/expm1 loop to stay bit-identical to the
// per-op f(a.*x ± b): the same hn:: on the SIMD body, the same std:: on the
// per-chunk scalar tail, and (in fusedTransAffine below) the same
// kTranscendentalThreshold so parallel_for produces the same chunk boundaries.
// The affine is Mul-then-Add (two roundings, NOT FMA) to match the affine
// fallback. exp/expm1 of any real is real → no complex-promotion domain.

#include <numkit/ops/fused_kernels.hpp>
#include <numkit/ops/parallel_for.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "fused_trans_affine_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
#include <hwy/contrib/math/math-inl.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// tan has no Highway primitive; numkit's TanLoop uses SLEEF's xtan kernel with a
// per-block |x| >= 1e6 scalar fallback. To stay bit-identical to the per-op
// tan(a.*x ± b) the fused kernel must mirror that on the inner. TanVec is copied
// verbatim from math/trig/trig_highway.cpp (SLEEF sleefsimddp.c, BSL-1.0) — ops/
// can't depend on math/, so the kernel is vendored here too.
template <class D, class DI>
HWY_INLINE hn::VFromD<D> TanVec(D d, DI di, hn::VFromD<D> v)
{
    const auto m2pi = hn::Set(d, 0.636619772367581343075535053490057448); // 2/pi
    auto dql = hn::Round(hn::Mul(v, m2pi));
    auto s = hn::MulAdd(dql, hn::Set(d, -3.141592653589793116 * 0.5), v);    // -PI_A2/2
    s = hn::MulAdd(dql, hn::Set(d, -1.2246467991473532072e-16 * 0.5), s);    // -PI_B2/2
    auto qlf = dql;
    const auto big = hn::Ge(hn::Abs(v), hn::Set(d, 15.0));
    if (!hn::AllFalse(d, big)) { // some lanes need the extended reduction
        auto dqh = hn::Mul(hn::Trunc(hn::Mul(v, hn::Set(d, 0.636619772367581343075535053490057448 / 16777216.0))),
                           hn::Set(d, 16777216.0));
        auto dqe = hn::Round(hn::Sub(hn::Mul(v, m2pi), dqh));
        auto u = hn::MulAdd(dqh, hn::Set(d, -3.1415926218032836914 * 0.5), v);
        u = hn::MulAdd(dqe, hn::Set(d, -3.1415926218032836914 * 0.5), u);
        u = hn::MulAdd(dqh, hn::Set(d, -3.1786509424591713469e-08 * 0.5), u);
        u = hn::MulAdd(dqe, hn::Set(d, -3.1786509424591713469e-08 * 0.5), u);
        u = hn::MulAdd(dqh, hn::Set(d, -1.2246467864107188502e-16 * 0.5), u);
        u = hn::MulAdd(dqe, hn::Set(d, -1.2246467864107188502e-16 * 0.5), u);
        u = hn::MulAdd(hn::Add(dqh, dqe), hn::Set(d, -1.2736634327021899816e-24 * 0.5), u);
        s = hn::IfThenElse(big, u, s);
        qlf = hn::IfThenElse(big, dqe, qlf);
    }
    const auto qi = hn::ConvertTo(di, qlf);
    const auto x = hn::Mul(s, hn::Set(d, 0.5));
    const auto s2 = hn::Mul(x, x);
    auto p = hn::Set(d, 0.3245098826639276316e-3);
    p = hn::MulAdd(p, s2, hn::Set(d, 0.5619219738114323735e-3));
    p = hn::MulAdd(p, s2, hn::Set(d, 0.1460781502402784494e-2));
    p = hn::MulAdd(p, s2, hn::Set(d, 0.3591611540792499519e-2));
    p = hn::MulAdd(p, s2, hn::Set(d, 0.8863268409563113126e-2));
    p = hn::MulAdd(p, s2, hn::Set(d, 0.2186948728185535498e-1));
    p = hn::MulAdd(p, s2, hn::Set(d, 0.5396825399517272970e-1));
    p = hn::MulAdd(p, s2, hn::Set(d, 0.1333333333330500581e+0));
    p = hn::MulAdd(p, s2, hn::Set(d, 0.3333333333333343695e+0));
    const auto u = hn::MulAdd(s2, hn::Mul(p, x), x);    // s2*(p*x) + x
    const auto y = hn::MulAdd(u, u, hn::Set(d, -1.0));  // u^2 - 1
    const auto xx = hn::Mul(u, hn::Set(d, -2.0));       // -2u
    const auto odd = hn::RebindMask(d, hn::Ne(hn::And(qi, hn::Set(di, std::int64_t(1))), hn::Zero(di)));
    auto r = hn::Div(hn::IfThenElse(odd, hn::Neg(y), xx), hn::IfThenElse(odd, xx, y));
    return hn::IfThenElse(hn::Eq(v, hn::Zero(d)), v, r); // tan(0)=0, tan(-0)=-0
}

// tan(inner), inner = affine (p0*x + p1) or shift-divide ((x - p0)/p1) per
// `divide`. Mirrors TanLoop: per Lanes-block, TanVec when all |inner| < 1e6 else
// std::tan on each lane; the scalar tail is std::tan. Block boundaries and inner
// values match the per-op tan(temp), so the per-block decision is identical.
void TanInnerLoop(const double *x, double p0, double p1, bool divide,
                  double *out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const hn::RebindToSigned<decltype(d)> di;
    const std::size_t L = hn::Lanes(d);
    const auto vp0 = hn::Set(d, p0), vp1 = hn::Set(d, p1);
    const auto rempiThreshold = hn::Set(d, 1.0e6);
    std::size_t i = 0;
    for (; i + L <= n; i += L) {
        const auto xv = hn::LoadU(d, x + i);
        const auto inner = divide ? hn::Div(hn::Sub(xv, vp0), vp1)
                                  : hn::Add(hn::Mul(xv, vp0), vp1);
        if (hn::AllTrue(d, hn::Lt(hn::Abs(inner), rempiThreshold))) {
            hn::StoreU(TanVec(d, di, inner), d, out + i);
        } else { // |inner| >= 1e6 or non-finite: scalar reference for the block
            for (std::size_t j = 0; j < L; ++j) {
                const double iv = divide ? (x[i + j] - p0) / p1 : p0 * x[i + j] + p1;
                out[i + j] = std::tan(iv);
            }
        }
    }
    for (; i < n; ++i)
        out[i] = std::tan(divide ? (x[i] - p0) / p1 : p0 * x[i] + p1);
}

// Vec maps (tag, inner-vector) → result vector (the Highway transcendental);
// Scalar maps the tail double → double (the matching libm call). The inner is
// affine (p0*x + p1) or shift-divide ((x - p0)/p1) per `divide` (loop-invariant
// → the affine path is unchanged).
template <class Vec, class Scalar>
void transLoop(const double *x, double p0, double p1, bool divide, double *out,
               std::size_t n, Vec vop, Scalar sop) {
    const hn::ScalableTag<double> d;
    const std::size_t L = hn::Lanes(d);
    const auto vp0 = hn::Set(d, p0), vp1 = hn::Set(d, p1);
    std::size_t i = 0;
    for (; i + L <= n; i += L) {
        const auto x_ = hn::LoadU(d, x + i);
        const auto v = divide ? hn::Div(hn::Sub(x_, vp0), vp1)
                              : hn::Add(hn::Mul(x_, vp0), vp1);
        hn::StoreU(vop(d, v), d, out + i);
    }
    for (; i < n; ++i)
        out[i] = sop(divide ? (x[i] - p0) / p1 : p0 * x[i] + p1);
}

void TransAffineImpl(const double *x, double p0, double p1, bool divide,
                     TransAffineFn fn, double *out, std::size_t n) {
    switch (fn) {
        case TransAffineFn::Exp:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Exp(d, v); },
                      [](double v) { return std::exp(v); });
            break;
        case TransAffineFn::Expm1:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Expm1(d, v); },
                      [](double v) { return std::expm1(v); });
            break;
        case TransAffineFn::Log:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Log(d, v); },
                      [](double v) { return std::log(v); });
            break;
        case TransAffineFn::Log2:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Log2(d, v); },
                      [](double v) { return std::log2(v); });
            break;
        case TransAffineFn::Log10:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Log10(d, v); },
                      [](double v) { return std::log10(v); });
            break;
        case TransAffineFn::Sin:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Sin(d, v); },
                      [](double v) { return std::sin(v); });
            break;
        case TransAffineFn::Cos:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Cos(d, v); },
                      [](double v) { return std::cos(v); });
            break;
        case TransAffineFn::Tanh:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Tanh(d, v); },
                      [](double v) { return std::tanh(v); });
            break;
        case TransAffineFn::Sinh:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Sinh(d, v); },
                      [](double v) { return std::sinh(v); });
            break;
        case TransAffineFn::Atan:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Atan(d, v); },
                      [](double v) { return std::atan(v); });
            break;
        case TransAffineFn::Asinh:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Asinh(d, v); },
                      [](double v) { return std::asinh(v); });
            break;
        case TransAffineFn::Asin:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Asin(d, v); },
                      [](double v) { return std::asin(v); });
            break;
        case TransAffineFn::Acos:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Acos(d, v); },
                      [](double v) { return std::acos(v); });
            break;
        case TransAffineFn::Acosh:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Acosh(d, v); },
                      [](double v) { return std::acosh(v); });
            break;
        case TransAffineFn::Atanh:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Atanh(d, v); },
                      [](double v) { return std::atanh(v); });
            break;
        case TransAffineFn::Log1p:
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) { return hn::Log1p(d, v); },
                      [](double v) { return std::log1p(v); });
            break;
        case TransAffineFn::Cosh:
            // No Highway Cosh — compose 0.5*(e^v + e^-v) exactly as CoshLoop.
            transLoop(x, p0, p1, divide, out, n,
                      [](auto d, auto v) {
                          return hn::Mul(hn::Set(d, 0.5),
                                         hn::Add(hn::Exp(d, v), hn::Exp(d, hn::Neg(v))));
                      },
                      [](double v) { return std::cosh(v); });
            break;
        case TransAffineFn::Tan:
            // Dedicated path (per-block 1e6 fallback can't fit the transLoop vop).
            TanInnerLoop(x, p0, p1, divide, out, n);
            break;
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::ops
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::ops {

HWY_EXPORT(TransAffineImpl);

void fusedTransAffine(const double *x, double scale, double offset,
                      TransAffineFn fn, double *out, std::size_t n) {
    if (n == 0) return;
    // kTranscendentalThreshold (not the arithmetic 1<<16) so the chunk
    // boundaries match numkit's exp/expm1 parallel_for — the per-chunk
    // hn::/std:: split must land on the same elements for bit-exactness.
    detail::parallel_for(n, detail::kTranscendentalThreshold,
                         [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(TransAffineImpl)(x + s, scale, offset, /*divide=*/false,
                                              fn, out + s, e - s);
    });
}

void fusedTransShiftDiv(const double *x, double sub, double div,
                        TransAffineFn fn, double *out, std::size_t n) {
    if (n == 0) return;
    detail::parallel_for(n, detail::kTranscendentalThreshold,
                         [&](std::size_t s, std::size_t e) {
        HWY_DYNAMIC_DISPATCH(TransAffineImpl)(x + s, sub, div, /*divide=*/true,
                                              fn, out + s, e - s);
    });
}

} // namespace numkit::ops
#endif // HWY_ONCE
