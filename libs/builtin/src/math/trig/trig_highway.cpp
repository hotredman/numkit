// libs/builtin/src/math/trig/trig_highway.cpp
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
#define HWY_TARGET_INCLUDE "math/trig/trig_highway.cpp"
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

// ── Atan2 / Hypot (binary) ───────────────────────────────────────────

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

// hypot(a, b) = sqrt(a² + b²). Highway has Sqrt but no Hypot;
// composing this way is fast but loses std::hypot's overflow-safe
// scaling for |a|, |b| > sqrt(DBL_MAX) ≈ 1.34e154. Scalar tail uses
// std::hypot. For typical inputs (well within the safe range) the
// SIMD path is correct to ULP and matches MATLAB.
void HypotLoop(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,
               double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto va = hn::LoadU(d, a + i);
        auto vb = hn::LoadU(d, b + i);
        hn::StoreU(hn::Sqrt(hn::MulAdd(va, va, hn::Mul(vb, vb))), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::hypot(a[i], b[i]);
}

// ── Degree variants: SIMD bulk path. ─────────────────────────────────
//
// MATLAB preserves exact zeros / ±1 at integer multiples of 90° on
// scalar input (e.g. sind(180) == 0 exactly, not ~1.2e-16). The
// public-API scalar fast-path (in trig_highway.cpp / trig_portable.cpp)
// keeps the snap. The SIMD vector path does NOT snap — for typical
// 1M-pt sweeps the inputs are not exact integer multiples of 90.
// (Bench-OK; if a downstream consumer needs vector-path exact-zero
// snapping, add a post-pass that overrides multiples of 90.)

namespace deg_consts {
constexpr double kDeg2Rad = 3.14159265358979323846 / 180.0;
constexpr double kRad2Deg = 180.0 / 3.14159265358979323846;
constexpr double kPi      = 3.14159265358979323846;
}

void SindLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto k = hn::Set(d, deg_consts::kDeg2Rad);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Sin(d, hn::Mul(v, k)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::sin(in[i] * deg_consts::kDeg2Rad);
}

void CosdLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto k = hn::Set(d, deg_consts::kDeg2Rad);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Cos(d, hn::Mul(v, k)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::cos(in[i] * deg_consts::kDeg2Rad);
}

void TandLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto k = hn::Set(d, deg_consts::kDeg2Rad);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        auto r = hn::Mul(v, k);
        hn::StoreU(hn::Div(hn::Sin(d, r), hn::Cos(d, r)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::tan(in[i] * deg_consts::kDeg2Rad);
}

void AsindLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto k = hn::Set(d, deg_consts::kRad2Deg);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Mul(hn::Asin(d, v), k), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::asin(in[i]) * deg_consts::kRad2Deg;
}

void AcosdLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto k = hn::Set(d, deg_consts::kRad2Deg);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Mul(hn::Acos(d, v), k), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::acos(in[i]) * deg_consts::kRad2Deg;
}

void AtandLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto k = hn::Set(d, deg_consts::kRad2Deg);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Mul(hn::Atan(d, v), k), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::atan(in[i]) * deg_consts::kRad2Deg;
}

void Atan2dLoop(const double *HWY_RESTRICT y, const double *HWY_RESTRICT x,
                double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto k = hn::Set(d, deg_consts::kRad2Deg);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto vy = hn::LoadU(d, y + i);
        auto vx = hn::LoadU(d, x + i);
        hn::StoreU(hn::Mul(hn::Atan2(d, vy, vx), k), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::atan2(y[i], x[i]) * deg_consts::kRad2Deg;
}

// ── Multiple-of-π variants. ──────────────────────────────────────────

void SinpiLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto k = hn::Set(d, deg_consts::kPi);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Sin(d, hn::Mul(v, k)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::sin(deg_consts::kPi * in[i]);
}

void CospiLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto k = hn::Set(d, deg_consts::kPi);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Cos(d, hn::Mul(v, k)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::cos(deg_consts::kPi * in[i]);
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
HWY_EXPORT(SindLoop);
HWY_EXPORT(CosdLoop);
HWY_EXPORT(TandLoop);
HWY_EXPORT(AsindLoop);
HWY_EXPORT(AcosdLoop);
HWY_EXPORT(AtandLoop);
HWY_EXPORT(Atan2dLoop);
HWY_EXPORT(SinpiLoop);
HWY_EXPORT(CospiLoop);
HWY_EXPORT(HypotLoop);

namespace {

// Constants for degree / multiple-of-π variants (scalar path).
// kept in the anonymous namespace so they don't leak.
constexpr double kDeg2Rad = 3.14159265358979323846 / 180.0;
constexpr double kRad2Deg = 180.0 / 3.14159265358979323846;
constexpr double kPi      = 3.14159265358979323846;

// Snap exact integer multiples of 90° to exact 0 / ±1 / ±Inf.
// Used by the scalar fast-path for sind / cosd / tand to match
// MATLAB exactly (e.g. sind(180) == 0 not ~1.2e-16). The vector
// SIMD path skips this — bench inputs rarely hit exact multiples.
inline double sind_scalar(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = std::fmod(x, 360.0);
    if (xr == 0.0 || xr == 180.0 || xr == -180.0) return 0.0;
    if (xr == 90.0)  return  1.0;
    if (xr == -90.0) return -1.0;
    return std::sin(xr * kDeg2Rad);
}

inline double cosd_scalar(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = std::fmod(x, 360.0);
    if (xr == 90.0 || xr == -90.0 || xr == 270.0 || xr == -270.0) return 0.0;
    if (xr == 0.0) return  1.0;
    if (xr == 180.0 || xr == -180.0) return -1.0;
    return std::cos(xr * kDeg2Rad);
}

inline double tand_scalar(double x)
{
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = std::fmod(x, 360.0);
    if (xr == 0.0 || xr == 180.0 || xr == -180.0) return 0.0;
    if (xr == 90.0)  return  std::numeric_limits<double>::infinity();
    if (xr == -90.0) return -std::numeric_limits<double>::infinity();
    if (xr == 270.0) return  std::numeric_limits<double>::infinity();
    if (xr == -270.0)return -std::numeric_limits<double>::infinity();
    return std::tan(xr * kDeg2Rad);
}

inline double sinpi_scalar(double x)
{
    if (std::isnan(x)) return x;
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = std::remainder(x, 2.0);
    if (xr == 0.0 || xr == 1.0 || xr == -1.0) return 0.0;
    if (xr ==  0.5) return  1.0;
    if (xr == -0.5) return -1.0;
    return std::sin(kPi * xr);
}

inline double cospi_scalar(double x)
{
    if (std::isnan(x)) return x;
    if (!std::isfinite(x)) return std::numeric_limits<double>::quiet_NaN();
    const double xr = std::remainder(x, 2.0);
    if (xr ==  0.5 || xr == -0.5) return 0.0;
    if (xr ==  0.0) return  1.0;
    if (xr ==  1.0 || xr == -1.0) return -1.0;
    return std::cos(kPi * xr);
}

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

// ── Degree variants + sinpi/cospi: SIMD-dispatched real-vector path ──
//
// Scalar path uses the snap-helpers above to preserve MATLAB's exact
// 0 / ±1 / ±Inf at integer multiples of 90 (degree variants) or ½
// (sinpi/cospi). Vector SIMD path multiplies-and-calls Sin/Cos/Atan
// without snapping; for typical bench inputs this is correct to ULP.

Value sind(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sin(c * kDeg2Rad); }, mr);
    if (x.isScalar())
        return Value::scalar(sind_scalar(x.toScalar()), mr);
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(SindLoop)(in, out, n);
        },
        sind_scalar,
        [](const Complex &c) { return std::sin(c * kDeg2Rad); });
}

Value cosd(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cos(c * kDeg2Rad); }, mr);
    if (x.isScalar())
        return Value::scalar(cosd_scalar(x.toScalar()), mr);
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(CosdLoop)(in, out, n);
        },
        cosd_scalar,
        [](const Complex &c) { return std::cos(c * kDeg2Rad); });
}

Value tand(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::tan(c * kDeg2Rad); }, mr);
    if (x.isScalar())
        return Value::scalar(tand_scalar(x.toScalar()), mr);
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(TandLoop)(in, out, n);
        },
        tand_scalar,
        [](const Complex &c) { return std::tan(c * kDeg2Rad); });
}

Value asind(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AsindLoop)(in, out, n);
        },
        [](double v) { return std::asin(v) * kRad2Deg; },
        [](const Complex &c) { return std::asin(c) * kRad2Deg; });
}

Value acosd(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AcosdLoop)(in, out, n);
        },
        [](double v) { return std::acos(v) * kRad2Deg; },
        [](const Complex &c) { return std::acos(c) * kRad2Deg; });
}

Value atand(std::pmr::memory_resource *mr, const Value &x)
{
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AtandLoop)(in, out, n);
        },
        [](double v) { return std::atan(v) * kRad2Deg; },
        [](const Complex &c) { return std::atan(c) * kRad2Deg; });
}

Value atan2d(std::pmr::memory_resource *mr, const Value &y, const Value &x)
{
    if (y.isComplex() || x.isComplex())
        return elementwiseDouble(y, x, [](double yy, double xx) { return std::atan2(yy, xx) * kRad2Deg; }, mr);
    if (y.isScalar() && x.isScalar())
        return Value::scalar(std::atan2(y.toScalar(), x.toScalar()) * kRad2Deg, mr);

    if (!y.isScalar() && !x.isScalar() && y.dims() == x.dims()) {
        Value r = createLike(y, ValueType::DOUBLE, mr);
        if (y.numel() == 0)
            return r;
        const double *yp  = y.doubleData();
        const double *xp  = x.doubleData();
        double       *out = r.doubleDataMut();
        numkit::detail::parallel_for(y.numel(), numkit::detail::kTranscendentalThreshold,
            [=](std::size_t s, std::size_t e) {
                HWY_DYNAMIC_DISPATCH(Atan2dLoop)(yp + s, xp + s, out + s, e - s);
            });
        return r;
    }
    return elementwiseDouble(y, x, [](double yy, double xx) { return std::atan2(yy, xx) * kRad2Deg; }, mr);
}

Value sinpi(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sin(kPi * c); }, mr);
    if (x.isScalar())
        return Value::scalar(sinpi_scalar(x.toScalar()), mr);
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(SinpiLoop)(in, out, n);
        },
        sinpi_scalar,
        [](const Complex &c) { return std::sin(kPi * c); });
}

Value cospi(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::cos(kPi * c); }, mr);
    if (x.isScalar())
        return Value::scalar(cospi_scalar(x.toScalar()), mr);
    return unaryRealDouble(
        mr, x, /*hint*/ nullptr,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(CospiLoop)(in, out, n);
        },
        cospi_scalar,
        [](const Complex &c) { return std::cos(kPi * c); });
}

// hypot binary: same shape pattern as atan2 — same-shape real path
// goes SIMD; mixed/scalar/complex falls back to std::hypot via the
// generic scaffold.
Value hypot(std::pmr::memory_resource *mr, const Value &a, const Value &b)
{
    if (a.isComplex() || b.isComplex())
        return elementwiseDouble(a, b, [](double aa, double bb) { return std::hypot(aa, bb); }, mr);
    if (a.isScalar() && b.isScalar())
        return Value::scalar(std::hypot(a.toScalar(), b.toScalar()), mr);

    if (!a.isScalar() && !b.isScalar() && a.dims() == b.dims()) {
        Value r = createLike(a, ValueType::DOUBLE, mr);
        if (a.numel() == 0)
            return r;
        const double *ap  = a.doubleData();
        const double *bp  = b.doubleData();
        double       *out = r.doubleDataMut();
        numkit::detail::parallel_for(a.numel(), numkit::detail::kTranscendentalThreshold,
            [=](std::size_t s, std::size_t e) {
                HWY_DYNAMIC_DISPATCH(HypotLoop)(ap + s, bp + s, out + s, e - s);
            });
        return r;
    }
    return elementwiseDouble(a, b, [](double aa, double bb) { return std::hypot(aa, bb); }, mr);
}

} // namespace numkit::builtin

#endif // HWY_ONCE
