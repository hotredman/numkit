// libs/builtin/src/math/trig/trig_simd.cpp
//
// Highway dynamic-dispatch trig family: sin, cos, sinh, cosh, tanh,
// asin, acos, atan, atan2, asinh, atanh. Each function gets its own
// HWY_EXPORT / HWY_DYNAMIC_DISPATCH pair; the HWY_NAMESPACE block up
// top holds the target-specific vector loops. Highway's
// hwy/contrib/math header provides the underlying Sin / Cos / Sinh /
// Tanh / Asin / Acos / Atan / Asinh / Atanh / Atan2 primitives
// (SLEEF-derived polynomial approximations; ULP <= 4 across all
// supported targets). Highway has no Cosh; we compose from Exp.
// Highway has no Tan; we compose from Sin/Cos (handled in trigonometry.cpp).
//
// The complex and scalar paths mirror trig_portable.cpp exactly —
// SIMD doesn't help there. Parity vs the scalar reference is
// verified in libs/builtin/tests/simd_parity_test.cpp.

#include <numkit/builtin/math/trig/trigonometry.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/parallel_for.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "math/trig/trig_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
#include <hwy/contrib/math/math-inl.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void SinLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Sin(d, v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::sin(in[i]);
}

void CosLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Cos(d, v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::cos(in[i]);
}

// ── Hyperbolic (Highway has Sinh and Tanh; Cosh is composed below) ───

void SinhLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Sinh(d, v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::sinh(in[i]);
}

void TanhLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Tanh(d, v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::tanh(in[i]);
}

// cosh(x) = (e^x + e^-x) / 2 — composed from Highway Exp.
void CoshLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    const auto half = hn::Set(d, 0.5);
    for (; i + N <= n; i += N) {
        auto v   = hn::LoadU(d, in + i);
        auto ep  = hn::Exp(d, v);
        auto en  = hn::Exp(d, hn::Neg(v));
        hn::StoreU(hn::Mul(half, hn::Add(ep, en)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::cosh(in[i]);
}

// ── Inverse trig ─────────────────────────────────────────────────────

void AsinLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Asin(d, v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::asin(in[i]);
}

void AcosLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Acos(d, v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::acos(in[i]);
}

void AtanLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Atan(d, v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::atan(in[i]);
}

void AsinhLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Asinh(d, v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::asinh(in[i]);
}

void AtanhLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Atanh(d, v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::atanh(in[i]);
}

// tan(x) = sin(x) / cos(x) — composed because Highway has no Tan.
void TanLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Div(hn::Sin(d, v), hn::Cos(d, v)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::tan(in[i]);
}

void AcoshLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Acosh(d, v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::acosh(in[i]);
}

// ── Atan2 (binary) ───────────────────────────────────────────────────

void Atan2Loop(const double *HWY_RESTRICT y, const double *HWY_RESTRICT x,
               double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto vy = hn::LoadU(d, y + i);
        auto vx = hn::LoadU(d, x + i);
        hn::StoreU(hn::Atan2(d, vy, vx), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::atan2(y[i], x[i]);
}

} // namespace HWY_NAMESPACE
} // namespace numkit::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::builtin {

HWY_EXPORT(SinLoop);
HWY_EXPORT(CosLoop);
HWY_EXPORT(SinhLoop);
HWY_EXPORT(CoshLoop);
HWY_EXPORT(TanhLoop);
HWY_EXPORT(AsinLoop);
HWY_EXPORT(AcosLoop);
HWY_EXPORT(AtanLoop);
HWY_EXPORT(AsinhLoop);
HWY_EXPORT(AtanhLoop);
HWY_EXPORT(Atan2Loop);
HWY_EXPORT(TanLoop);
HWY_EXPORT(AcoshLoop);

namespace {

// Shared shape for sin/cos: delegate complex / scalar to the
// reference path, route real vectors through the dispatcher. When
// `hint` is a uniquely-owned heap double of matching shape, steal
// its buffer instead of allocating a fresh result — saves the
// per-call N-element mr that dominates at large N. See the
// docblock on abs() in math/elementary/misc.hpp for the full hint contract.
template <typename LoopDispatch, typename ScalarOp, typename ComplexOp>
Value unaryRealDouble(std::pmr::memory_resource *mr, const Value &x, Value *hint,
                       LoopDispatch loop, ScalarOp scalarOp, ComplexOp complexOp)
{
    if (x.isComplex()) {
        if (x.isScalar())
            return Value::complexScalar(complexOp(x.toComplex()), mr);
        return unaryComplex(x, complexOp, mr);
    }
    if (x.isScalar())
        return Value::scalar(scalarOp(x.toScalar()), mr);

    Value r;
    if (hint && hint->isHeapDouble() && hint->heapRefCount() == 1
        && hint->dims() == x.dims()) {
        r = std::move(*hint);
    } else {
        r = createLike(x, ValueType::DOUBLE, mr);
    }
    // Empty inputs may not have a DOUBLE backing buffer (e.g. `[]`
    // stored as a tag); skip the loop entirely. createLike already
    // produced an empty result of the correct dims/type.
    if (x.numel() == 0)
        return r;

    const double *in  = x.doubleData();
    double       *out = r.doubleDataMut();
    // Transcendentals are heavier per element than +/-/.* — pays off
    // earlier, hence the smaller threshold.
    numkit::detail::parallel_for(x.numel(), numkit::detail::kTranscendentalThreshold,
        [=](std::size_t s, std::size_t e) {
            loop(in + s, out + s, e - s);
        });
    return r;
}

} // namespace

Value sin(std::pmr::memory_resource *mr, const Value &x, Value *hint)
{
    return unaryRealDouble(
        mr, x, hint,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(SinLoop)(in, out, n);
        },
        [](double v) { return std::sin(v); },
        [](const Complex &c) { return std::sin(c); });
}

Value cos(std::pmr::memory_resource *mr, const Value &x, Value *hint)
{
    return unaryRealDouble(
        mr, x, hint,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(CosLoop)(in, out, n);
        },
        [](double v) { return std::cos(v); },
        [](const Complex &c) { return std::cos(c); });
}

// ── Hyperbolic + inverse trig: SIMD-dispatched real-vector path. ─────
//
// Wrap unaryRealDouble; complex / scalar paths delegate to std::,
// matching the portable backend bit-for-bit.

Value sinh(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(SinhLoop)(in, out, n);
        },
        [](double v) { return std::sinh(v); },
        [](const Complex &c) { return std::sinh(c); });
}

Value cosh(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(CoshLoop)(in, out, n);
        },
        [](double v) { return std::cosh(v); },
        [](const Complex &c) { return std::cosh(c); });
}

Value tanh(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(TanhLoop)(in, out, n);
        },
        [](double v) { return std::tanh(v); },
        [](const Complex &c) { return std::tanh(c); });
}

Value asin(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AsinLoop)(in, out, n);
        },
        [](double v) { return std::asin(v); },
        [](const Complex &c) { return std::asin(c); });
}

Value acos(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AcosLoop)(in, out, n);
        },
        [](double v) { return std::acos(v); },
        [](const Complex &c) { return std::acos(c); });
}

Value atan(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AtanLoop)(in, out, n);
        },
        [](double v) { return std::atan(v); },
        [](const Complex &c) { return std::atan(c); });
}

Value asinh(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AsinhLoop)(in, out, n);
        },
        [](double v) { return std::asinh(v); },
        [](const Complex &c) { return std::asinh(c); });
}

Value atanh(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atanh(c); }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v < -1.0 || v > 1.0)
            return Value::complexScalar(std::atanh(Complex(v, 0.0)), mr);
    }
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AtanhLoop)(in, out, n);
        },
        [](double v) { return std::atanh(v); },
        [](const Complex &c) { return std::atanh(c); });
}

Value tan(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(TanLoop)(in, out, n);
        },
        [](double v) { return std::tan(v); },
        [](const Complex &c) { return std::tan(c); });
}

Value acosh(std::pmr::memory_resource *mr, const Value &x)
{
    // MATLAB promotes a scalar |x|<1 to complex (so acosh(0.5) → 1.0472i,
    // not NaN). Vector path matches std::acosh — NaN for out-of-domain.
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acosh(c); }, mr);
    if (x.isScalar() && x.toScalar() < 1.0)
        return Value::complexScalar(std::acosh(Complex(x.toScalar(), 0.0)), mr);
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AcoshLoop)(in, out, n);
        },
        [](double v) { return std::acosh(v); },
        [](const Complex &c) { return std::acosh(c); });
}

// atan2 is binary: y, x → P. Real same-shape case uses the SIMD
// loop; mixed/broadcast/complex cases fall through to the generic
// scalar scaffold.
Value atan2(std::pmr::memory_resource *mr, const Value &y, const Value &x)
{
    if (y.isComplex() || x.isComplex())
        return elementwiseDouble(y, x, [](double yy, double xx) { return std::atan2(yy, xx); }, mr);
    if (y.isScalar() && x.isScalar())
        return Value::scalar(std::atan2(y.toScalar(), x.toScalar()), mr);

    if (!y.isScalar() && !x.isScalar() && y.dims() == x.dims()) {
        Value r = createLike(y, ValueType::DOUBLE, mr);
        if (y.numel() == 0)
            return r;
        const double *yp  = y.doubleData();
        const double *xp  = x.doubleData();
        double       *out = r.doubleDataMut();
        numkit::detail::parallel_for(y.numel(), numkit::detail::kTranscendentalThreshold,
            [=](std::size_t s, std::size_t e) {
                HWY_DYNAMIC_DISPATCH(Atan2Loop)(yp + s, xp + s, out + s, e - s);
            });
        return r;
    }
    return elementwiseDouble(y, x, [](double yy, double xx) { return std::atan2(yy, xx); }, mr);
}

} // namespace numkit::builtin

#endif // HWY_ONCE
