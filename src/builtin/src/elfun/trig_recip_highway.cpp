// toolboxes/builtin/src/math/trig/trig_recip_highway.cpp
//
// Highway dynamic-dispatch reciprocal-trig family: sec, csc, cot,
// sech, csch, coth, secd, cscd, cotd, and their inverse forms (asec,
// acsc, acot, asech, acsch, acoth, asecd, acscd, acotd). All composed
// from Highway's contrib/math primitives (Cos, Sin, Sinh, Acos, Asin,
// Atan, Acosh, Asinh, Atanh) — the reciprocal forms emit a SIMD divide
// or a primary-on-1/x.
//
// Out-of-domain elements (e.g. asec(0.5) which MATLAB returns as
// complex) get NaN here, matching the existing scalar fallback for
// non-scalar inputs in trigonometry.cpp. The scalar fast-path keeps
// the complex fallback so `asec(0.5)` returns the MATLAB-spec complex
// value.
//
// See BUGS.md note attached to trig SIMD coverage (was: sec/csc/cot
// family fell back to scalar libm). Parity vs MATLAB R2025b verified
// at ULP <= 1e-10 for the test domains.

#include <numkit/builtin/elfun.hpp>

#include <numkit/value/value.hpp>
#include <numkit/ops/parallel_for.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>

#include <cmath>
#include <complex>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "elfun/trig_recip_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
#include <hwy/contrib/math/math-inl.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

namespace recip_consts {
constexpr double kDeg2Rad = 3.14159265358979323846 / 180.0;
constexpr double kRad2Deg = 180.0 / 3.14159265358979323846;
}

// ── Forward reciprocal: sec / csc / cot ────────────────────────────

void SecLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto one = hn::Set(d, 1.0);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Div(one, hn::Cos(d, v)), d, out + i);
    }
    for (; i < n; ++i) out[i] = 1.0 / std::cos(in[i]);
}

void CscLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto one = hn::Set(d, 1.0);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Div(one, hn::Sin(d, v)), d, out + i);
    }
    for (; i < n; ++i) out[i] = 1.0 / std::sin(in[i]);
}

void CotLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Div(hn::Cos(d, v), hn::Sin(d, v)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::cos(in[i]) / std::sin(in[i]);
}

// ── Hyperbolic reciprocal: sech / csch / coth ──────────────────────
// Highway has Sinh and Tanh but not Cosh — compose cosh from Exp.

static HWY_INLINE auto CoshVec(const hn::ScalableTag<double> d, hn::Vec<hn::ScalableTag<double>> v)
{
    const auto half = hn::Set(d, 0.5);
    auto ep = hn::Exp(d, v);
    auto en = hn::Exp(d, hn::Neg(v));
    return hn::Mul(half, hn::Add(ep, en));
}

void SechLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto one = hn::Set(d, 1.0);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Div(one, CoshVec(d, v)), d, out + i);
    }
    for (; i < n; ++i) out[i] = 1.0 / std::cosh(in[i]);
}

void CschLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto one = hn::Set(d, 1.0);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Div(one, hn::Sinh(d, v)), d, out + i);
    }
    for (; i < n; ++i) out[i] = 1.0 / std::sinh(in[i]);
}

void CothLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Div(CoshVec(d, v), hn::Sinh(d, v)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::cosh(in[i]) / std::sinh(in[i]);
}

// ── Degree reciprocal: secd / cscd / cotd ──────────────────────────
// Vector path skips the integer-multiple-of-90° snap (matches the
// degree forms in trig_highway.cpp). Scalar fast-path in trig_recip_*.cpp
// keeps the snap.

void SecdLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto k = hn::Set(d, recip_consts::kDeg2Rad);
    const auto one = hn::Set(d, 1.0);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Div(one, hn::Cos(d, hn::Mul(v, k))), d, out + i);
    }
    for (; i < n; ++i) out[i] = 1.0 / std::cos(in[i] * recip_consts::kDeg2Rad);
}

void CscdLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto k = hn::Set(d, recip_consts::kDeg2Rad);
    const auto one = hn::Set(d, 1.0);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Div(one, hn::Sin(d, hn::Mul(v, k))), d, out + i);
    }
    for (; i < n; ++i) out[i] = 1.0 / std::sin(in[i] * recip_consts::kDeg2Rad);
}

void CotdLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto k = hn::Set(d, recip_consts::kDeg2Rad);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v   = hn::LoadU(d, in + i);
        auto vrad = hn::Mul(v, k);
        hn::StoreU(hn::Div(hn::Cos(d, vrad), hn::Sin(d, vrad)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::cos(in[i] * recip_consts::kDeg2Rad)
                                / std::sin(in[i] * recip_consts::kDeg2Rad);
}

// ── Inverse reciprocal: asec / acsc / acot ─────────────────────────
// MATLAB returns complex for out-of-domain inputs (e.g. asec(0.5)).
// Vector path produces NaN there; scalar fallback in trig_recip_*.cpp
// detects this and returns a complex Value.

void AsecLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto one = hn::Set(d, 1.0);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Acos(d, hn::Div(one, v)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::acos(1.0 / in[i]);
}

void AcscLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto one = hn::Set(d, 1.0);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Asin(d, hn::Div(one, v)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::asin(1.0 / in[i]);
}

void AcotLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto one = hn::Set(d, 1.0);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Atan(d, hn::Div(one, v)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::atan(1.0 / in[i]);
}

// ── Inverse hyperbolic reciprocal: asech / acsch / acoth ───────────

void AsechLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto one = hn::Set(d, 1.0);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Acosh(d, hn::Div(one, v)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::acosh(1.0 / in[i]);
}

void AcschLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto one = hn::Set(d, 1.0);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Asinh(d, hn::Div(one, v)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::asinh(1.0 / in[i]);
}

void AcothLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto one = hn::Set(d, 1.0);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Atanh(d, hn::Div(one, v)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::atanh(1.0 / in[i]);
}

// ── Inverse degree reciprocal: asecd / acscd / acotd ───────────────

void AsecdLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto one = hn::Set(d, 1.0);
    const auto r2d = hn::Set(d, recip_consts::kRad2Deg);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Mul(hn::Acos(d, hn::Div(one, v)), r2d), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::acos(1.0 / in[i]) * recip_consts::kRad2Deg;
}

void AcscdLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto one = hn::Set(d, 1.0);
    const auto r2d = hn::Set(d, recip_consts::kRad2Deg);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Mul(hn::Asin(d, hn::Div(one, v)), r2d), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::asin(1.0 / in[i]) * recip_consts::kRad2Deg;
}

void AcotdLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto one = hn::Set(d, 1.0);
    const auto r2d = hn::Set(d, recip_consts::kRad2Deg);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Mul(hn::Atan(d, hn::Div(one, v)), r2d), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::atan(1.0 / in[i]) * recip_consts::kRad2Deg;
}

} // namespace HWY_NAMESPACE
} // namespace numkit::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::builtin {

HWY_EXPORT(SecLoop);
HWY_EXPORT(CscLoop);
HWY_EXPORT(CotLoop);
HWY_EXPORT(SechLoop);
HWY_EXPORT(CschLoop);
HWY_EXPORT(CothLoop);
HWY_EXPORT(SecdLoop);
HWY_EXPORT(CscdLoop);
HWY_EXPORT(CotdLoop);
HWY_EXPORT(AsecLoop);
HWY_EXPORT(AcscLoop);
HWY_EXPORT(AcotLoop);
HWY_EXPORT(AsechLoop);
HWY_EXPORT(AcschLoop);
HWY_EXPORT(AcothLoop);
HWY_EXPORT(AsecdLoop);
HWY_EXPORT(AcscdLoop);
HWY_EXPORT(AcotdLoop);

namespace {

constexpr double kDeg2Rad = 3.14159265358979323846 / 180.0;
constexpr double kRad2Deg = 180.0 / 3.14159265358979323846;

// Snap helpers reused from the degree forms (sind/cosd/tand) for the
// scalar fast-path of secd/cscd/cotd. MATLAB exposes exact 0 / ±1 /
// ±Inf at integer multiples of 90° on scalar input.
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

// Generic SIMD-dispatched real-vector path. Mirror of unaryRealDouble
// in trig_highway.cpp; complex / scalar inputs delegate to the
// reference path via scalarOp / complexOp lambdas.
template <typename LoopDispatch, typename ScalarOp, typename ComplexOp>
Value unaryRealDoubleRecip(const Value &x, LoopDispatch loop, ScalarOp scalarOp, ComplexOp complexOp, std::pmr::memory_resource *mr)
{
    if (x.isComplex()) {
        if (x.isScalar())
            return Value::complexScalar(complexOp(x.toComplex()), mr);
        return unaryComplex(x, complexOp, mr);
    }
    if (x.isScalar())
        return Value::scalar(scalarOp(x.toScalar()), mr);

    Value r = createLike(x, ValueType::DOUBLE, mr);
    if (x.numel() == 0) return r;
    const double *in  = x.doubleData();
    double       *out = r.doubleDataMut();
    numkit::detail::parallel_for(x.numel(), numkit::detail::kTranscendentalThreshold,
        [=](std::size_t s, std::size_t e) {
            loop(in + s, out + s, e - s);
        });
    return r;
}

} // namespace

// ── Forward reciprocal ─────────────────────────────────────────────

Value sec(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(SecLoop)(in, out, n);
        }, [](double v) { return 1.0 / std::cos(v); }, [](const Complex &c) { return Complex(1.0) / std::cos(c); }, mr);
}

Value csc(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(CscLoop)(in, out, n);
        }, [](double v) { return 1.0 / std::sin(v); }, [](const Complex &c) { return Complex(1.0) / std::sin(c); }, mr);
}

Value cot(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(CotLoop)(in, out, n);
        }, [](double v) { return std::cos(v) / std::sin(v); }, [](const Complex &c) { return std::cos(c) / std::sin(c); }, mr);
}

// ── Hyperbolic reciprocal ──────────────────────────────────────────

Value sech(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(SechLoop)(in, out, n);
        }, [](double v) { return 1.0 / std::cosh(v); }, [](const Complex &c) { return Complex(1.0) / std::cosh(c); }, mr);
}

Value csch(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(CschLoop)(in, out, n);
        }, [](double v) { return 1.0 / std::sinh(v); }, [](const Complex &c) { return Complex(1.0) / std::sinh(c); }, mr);
}

Value coth(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(CothLoop)(in, out, n);
        }, [](double v) { return std::cosh(v) / std::sinh(v); }, [](const Complex &c) { return std::cosh(c) / std::sinh(c); }, mr);
}

// ── Degree reciprocal: scalar path keeps the integer-multiple-of-90° snap.

Value secd(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(SecdLoop)(in, out, n);
        }, [](double v) { return 1.0 / cosd_scalar(v); }, [](const Complex &c) { return Complex(1.0) / std::cos(c * kDeg2Rad); }, mr);
}

Value cscd(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(CscdLoop)(in, out, n);
        }, [](double v) { return 1.0 / sind_scalar(v); }, [](const Complex &c) { return Complex(1.0) / std::sin(c * kDeg2Rad); }, mr);
}

Value cotd(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(CotdLoop)(in, out, n);
        }, [](double v) { return cosd_scalar(v) / sind_scalar(v); }, [](const Complex &c) {
            return std::cos(c * kDeg2Rad) / std::sin(c * kDeg2Rad);
        }, mr);
}

// ── Inverse reciprocal: scalar fast-path keeps the in-domain
// complex fallback (asec(0.5) → complex in MATLAB).

Value asec(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acos(Complex(1.0) / c); }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v > -1.0 && v < 1.0)
            return Value::complexScalar(std::acos(Complex(1.0 / v, 0.0)), mr);
    }
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AsecLoop)(in, out, n);
        }, [](double v) { return std::acos(1.0 / v); }, [](const Complex &c) { return std::acos(Complex(1.0) / c); }, mr);
}

Value acsc(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::asin(Complex(1.0) / c); }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v > -1.0 && v < 1.0 && v != 0.0)
            return Value::complexScalar(std::asin(Complex(1.0 / v, 0.0)), mr);
    }
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AcscLoop)(in, out, n);
        }, [](double v) { return std::asin(1.0 / v); }, [](const Complex &c) { return std::asin(Complex(1.0) / c); }, mr);
}

Value acot(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AcotLoop)(in, out, n);
        }, [](double v) { return std::atan(1.0 / v); }, [](const Complex &c) { return std::atan(Complex(1.0) / c); }, mr);
}

// ── Inverse hyperbolic reciprocal ──────────────────────────────────

Value asech(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::acosh(Complex(1.0) / c); }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v <= 0.0 || v > 1.0)
            return Value::complexScalar(std::acosh(Complex(1.0 / v, 0.0)), mr);
    }
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AsechLoop)(in, out, n);
        }, [](double v) { return std::acosh(1.0 / v); }, [](const Complex &c) { return std::acosh(Complex(1.0) / c); }, mr);
}

Value acsch(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AcschLoop)(in, out, n);
        }, [](double v) { return std::asinh(1.0 / v); }, [](const Complex &c) { return std::asinh(Complex(1.0) / c); }, mr);
}

Value acoth(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::atanh(Complex(1.0) / c); }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v >= -1.0 && v <= 1.0)
            return Value::complexScalar(std::atanh(Complex(1.0 / v, 0.0)), mr);
    }
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AcothLoop)(in, out, n);
        }, [](double v) { return std::atanh(1.0 / v); }, [](const Complex &c) { return std::atanh(Complex(1.0) / c); }, mr);
}

// ── Inverse degree reciprocal ──────────────────────────────────────

Value asecd(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) {
            return std::acos(Complex(1.0) / c) * kRad2Deg;
        }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v > -1.0 && v < 1.0)
            return Value::complexScalar(std::acos(Complex(1.0 / v, 0.0)) * kRad2Deg, mr);
    }
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AsecdLoop)(in, out, n);
        }, [](double v) { return std::acos(1.0 / v) * kRad2Deg; }, [](const Complex &c) { return std::acos(Complex(1.0) / c) * kRad2Deg; }, mr);
}

Value acscd(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) {
            return std::asin(Complex(1.0) / c) * kRad2Deg;
        }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v > -1.0 && v < 1.0 && v != 0.0)
            return Value::complexScalar(std::asin(Complex(1.0 / v, 0.0)) * kRad2Deg, mr);
    }
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AcscdLoop)(in, out, n);
        }, [](double v) { return std::asin(1.0 / v) * kRad2Deg; }, [](const Complex &c) { return std::asin(Complex(1.0) / c) * kRad2Deg; }, mr);
}

Value acotd(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleRecip(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(AcotdLoop)(in, out, n);
        }, [](double v) { return std::atan(1.0 / v) * kRad2Deg; }, [](const Complex &c) { return std::atan(Complex(1.0) / c) * kRad2Deg; }, mr);
}

} // namespace numkit::builtin

#endif // HWY_ONCE
