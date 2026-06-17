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

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "fused_trans_affine_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
#include <hwy/contrib/math/math-inl.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

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
