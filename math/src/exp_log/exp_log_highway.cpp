// toolboxes/builtin/src/math/exp_log/exp_log_highway.cpp
//
// Highway dynamic-dispatch exp / log. Each function gets its own
// HWY_EXPORT / HWY_DYNAMIC_DISPATCH pair; the HWY_NAMESPACE block up
// top holds the target-specific vector loops. Highway's
// hwy/contrib/math header provides the underlying Exp / Log
// primitives (SLEEF-derived polynomial approximations; ULP <= 4
// across all supported targets).
//
// The complex and scalar paths mirror exp_log_portable.cpp exactly —
// SIMD doesn't help there. Parity vs the scalar reference is
// verified in toolboxes/builtin/tests/simd_parity_test.cpp.

#include <numkit/builtin/math/exp_log/exponents.hpp>

#include <numkit/value/value.hpp>
#include <numkit/ops/parallel_for.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "exp_log/exp_log_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
#include <hwy/contrib/math/math-inl.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void ExpLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Exp(d, v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::exp(in[i]);
}

void LogLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Log(d, v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::log(in[i]);
}

// Tier-2 wrappers over Highway's contrib/math primitives (SLEEF-derived,
// ULP <= ~4). expm1 / log1p / log2 had no SIMD path before — they were
// scalar std::* calls in exponents.cpp.
void Expm1Loop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N)
        hn::StoreU(hn::Expm1(d, hn::LoadU(d, in + i)), d, out + i);
    for (; i < n; ++i) out[i] = std::expm1(in[i]);
}

void Log1pLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N)
        hn::StoreU(hn::Log1p(d, hn::LoadU(d, in + i)), d, out + i);
    for (; i < n; ++i) out[i] = std::log1p(in[i]);
}

void Log2Loop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N)
        hn::StoreU(hn::Log2(d, hn::LoadU(d, in + i)), d, out + i);
    for (; i < n; ++i) out[i] = std::log2(in[i]);
}

// log10 had no SIMD path before — it was a scalar std::log10 in
// exponents.cpp, ~10x slower than the vectorised log. reallog reuses
// LogLoop (it is log with a negative-input domain guard).
void Log10Loop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N)
        hn::StoreU(hn::Log10(d, hn::LoadU(d, in + i)), d, out + i);
    for (; i < n; ++i) out[i] = std::log10(in[i]);
}

// sqrt / realsqrt were scalar std::sqrt in exponents.cpp. hn::Sqrt maps to
// the hardware vsqrtpd, so the SIMD body is memory-bandwidth bound.
void SqrtLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N)
        hn::StoreU(hn::Sqrt(hn::LoadU(d, in + i)), d, out + i);
    for (; i < n; ++i) out[i] = std::sqrt(in[i]);
}

// pow2 (2^x). Highway has no Exp2 primitive, so this ports SLEEF's xexp2:
// q = rint(x); s = x - q in [-0.5, 0.5]; a single-double minimax polynomial
// (coefficients from SLEEF sleefsimddp.c, BSL-1.0) gives 2^s; ldexp by the
// integer q (two-step pow-of-two multiply, overflow-safe) gives 2^x. The
// integer reduction makes 2^n exact, matching MATLAB pow2 / std::exp2.
void Exp2Loop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const hn::RebindToSigned<decltype(d)> di;
    const std::size_t N = hn::Lanes(d);
    const auto bias = hn::Set(di, std::int64_t(1023));
    // 2^q via IEEE exponent bits: bitcast((q + 1023) << 52).
    auto pow2i = [&](decltype(hn::Zero(di)) q) {
        return hn::BitCast(d, hn::ShiftLeft<52>(hn::Add(q, bias)));
    };
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto x  = hn::LoadU(d, in + i);
        auto qd = hn::Round(x);              // nearest integer (ties to even)
        auto qi = hn::ConvertTo(di, qd);
        auto s  = hn::Sub(x, qd);            // |s| <= 0.5
        // 2^s, single-double Horner (degree 11): POLY10 + ln2 + 1.
        auto p = hn::Set(d, 0.4434359082926529454e-9);
        p = hn::MulAdd(p, s, hn::Set(d, 0.7073164598085707425e-8));
        p = hn::MulAdd(p, s, hn::Set(d, 0.1017819260921760451e-6));
        p = hn::MulAdd(p, s, hn::Set(d, 0.1321543872511327615e-5));
        p = hn::MulAdd(p, s, hn::Set(d, 0.1525273353517584730e-4));
        p = hn::MulAdd(p, s, hn::Set(d, 0.1540353045101147808e-3));
        p = hn::MulAdd(p, s, hn::Set(d, 0.1333355814670499073e-2));
        p = hn::MulAdd(p, s, hn::Set(d, 0.9618129107597600536e-2));
        p = hn::MulAdd(p, s, hn::Set(d, 0.5550410866482046596e-1));
        p = hn::MulAdd(p, s, hn::Set(d, 0.2402265069591012214e+0));
        p = hn::MulAdd(p, s, hn::Set(d, 0.6931471805599452862e+0)); // ln2
        p = hn::MulAdd(p, s, hn::Set(d, 1.0));
        // ldexp2(p, q) = p * 2^(q>>1) * 2^(q - q>>1)  (overflow-safe split).
        auto q1 = hn::ShiftRight<1>(qi);
        auto r  = hn::Mul(hn::Mul(p, pow2i(q1)), pow2i(hn::Sub(qi, q1)));
        // x >= 1024 -> +Inf; x < -2000 -> 0 (matches SLEEF/MATLAB edges).
        r = hn::IfThenElse(hn::Ge(x, hn::Set(d, 1024.0)),
                           hn::Set(d, std::numeric_limits<double>::infinity()), r);
        r = hn::IfThenZeroElse(hn::Lt(x, hn::Set(d, -2000.0)), r);
        hn::StoreU(r, d, out + i);
    }
    for (; i < n; ++i) out[i] = std::exp2(in[i]);
}

} // namespace HWY_NAMESPACE
} // namespace numkit::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::builtin {

HWY_EXPORT(ExpLoop);
HWY_EXPORT(LogLoop);
HWY_EXPORT(Expm1Loop);
HWY_EXPORT(Log1pLoop);
HWY_EXPORT(Log2Loop);
HWY_EXPORT(Log10Loop);
HWY_EXPORT(SqrtLoop);
HWY_EXPORT(Exp2Loop);

namespace {

// Shared shape for exp/log: delegate complex / scalar to the
// reference path, route real vectors through the dispatcher. When
// `hint` is a uniquely-owned heap double of matching shape, steal
// its buffer instead of allocating a fresh result — saves the
// per-call N-element mr that dominates at large N. See the
// docblock on abs() in math/elementary/misc.hpp for the full hint contract.
template <typename LoopDispatch, typename ScalarOp, typename ComplexOp>
Value unaryRealDouble(const Value &x, Value *hint, LoopDispatch loop, ScalarOp scalarOp, ComplexOp complexOp, std::pmr::memory_resource *mr)
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
    // stored as a tag); skip the loop entirely.
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

// expm1 / log1p / log2 are real-only. Route real DOUBLE arrays through the
// SIMD dispatch; keep scalars / complex / other element types on the
// reference scalar path (unaryDouble preserves type promotion + edge cases,
// bit-for-bit with the previous std::* implementation).
template <typename LoopDispatch, typename ScalarOp>
Value unaryRealArray(const Value &x, LoopDispatch loop, ScalarOp scalarOp,
                     std::pmr::memory_resource *mr)
{
    if (x.isComplex() || x.isScalar() || x.type() != ValueType::DOUBLE)
        return unaryDouble(x, scalarOp, mr);
    Value r = createLike(x, ValueType::DOUBLE, mr);
    if (x.numel() == 0)
        return r;
    const double *in  = x.doubleData();
    double       *out = r.doubleDataMut();
    numkit::detail::parallel_for(x.numel(), numkit::detail::kTranscendentalThreshold,
        [=](std::size_t s, std::size_t e) {
            loop(in + s, out + s, e - s);
        });
    return r;
}

} // namespace

Value exp(const Value &x, Value *hint, std::pmr::memory_resource *mr)
{
    return unaryRealDouble(x, hint, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(ExpLoop)(in, out, n);
        }, [](double v) { return std::exp(v); }, [](const Complex &c) { return std::exp(c); }, mr);
}

// log: MATLAB promotes a *scalar* negative input to complex (so
// log(-1) → i·π), but the element-wise path on a real vector just
// produces NaN for negatives — same as std::log. The SIMD Log()
// mirrors that behaviour.
static bool anyNegative(const Value &x);  // defined below (shared with sqrt)

Value log(const Value &x, Value *hint, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::log(c); }, mr);
    if (x.isScalar() && x.toScalar() < 0)
        return Value::complexScalar(std::log(Complex(x.toScalar(), 0.0)), mr);
    if (x.isScalar())
        return Value::scalar(std::log(x.toScalar()), mr);
    // Real array with any negative element -> promote the whole array to
    // complex (MATLAB: log([-1 1]) = [pi*i 0]). std::log's branch matches MATLAB.
    if (anyNegative(x)) {
        Value cx = x; cx.promoteToComplex(mr);
        return unaryComplex(cx, [](const Complex &c) { return std::log(c); }, mr);
    }

    Value r;
    if (hint && hint->isHeapDouble() && hint->heapRefCount() == 1
        && hint->dims() == x.dims()) {
        r = std::move(*hint);
    } else {
        r = createLike(x, ValueType::DOUBLE, mr);
    }
    if (x.numel() == 0)
        return r;
    const double *in  = x.doubleData();
    double       *out = r.doubleDataMut();
    numkit::detail::parallel_for(x.numel(), numkit::detail::kTranscendentalThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(LogLoop)(in + s, out + s, e - s);
        });
    return r;
}

Value expm1(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealArray(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(Expm1Loop)(in, out, n);
        }, [](double v) { return std::expm1(v); }, mr);
}

// log1p of a real x as a (possibly) complex value, accurate per element:
//   x >= -1 : log1p(x)  (real; -Inf at x == -1)
//   x <  -1 : 1+x < 0 → log|1+x| + i·π   (MATLAB principal branch)
static inline Complex log1pRealToComplex(double x)
{
    constexpr double kPi = 3.141592653589793;
    if (x >= -1.0) return Complex(std::log1p(x), 0.0);
    return Complex(std::log(-(1.0 + x)), kPi);
}
static inline bool anyLessThanMinusOne(const Value &x)
{
    const std::size_t n = x.numel();
    for (std::size_t i = 0; i < n; ++i)
        if (x.elemAsDouble(i) < -1.0) return true;
    return false;
}

// log1p(x) = log(1+x). For x < -1 the argument 1+x is negative, so MATLAB
// returns a complex value (log1p(-2) = log(-1) = i·π) and any element < -1
// promotes the whole real array to complex. Complex input uses log(1+z). The
// real (x >= -1) path keeps the accurate libm/SIMD log1p (so log1p(1e-15) is
// 1e-15, not the lossy log(1+1e-15)).
Value log1p(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) {
            return std::log(Complex(1.0, 0.0) + c); }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v < -1.0) return Value::complexScalar(log1pRealToComplex(v), mr);
        return Value::scalar(std::log1p(v), mr);
    }
    if (anyLessThanMinusOne(x)) {
        Value r = createLike(x, ValueType::COMPLEX, mr);
        Complex *out = r.complexDataMut();
        const std::size_t n = x.numel();
        for (std::size_t i = 0; i < n; ++i)
            out[i] = log1pRealToComplex(x.elemAsDouble(i));
        return r;
    }
    return unaryRealArray(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(Log1pLoop)(in, out, n);
        }, [](double v) { return std::log1p(v); }, mr);
}

// log2 / log10 of a negative scalar promote to complex like log
// (MATLAB: log2(-1) == 4.532i, log10(-1) == 1.364i). log2(z) = log(z)/log(2).
Value log2(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::log(c) / std::log(2.0); }, mr);
    if (x.isScalar() && x.toScalar() < 0.0)
        return Value::complexScalar(std::log(Complex(x.toScalar(), 0.0)) / std::log(2.0), mr);
    if (anyNegative(x)) {
        Value cx = x; cx.promoteToComplex(mr);
        return unaryComplex(cx, [](const Complex &c) { return std::log(c) / std::log(2.0); }, mr);
    }
    return unaryRealArray(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(Log2Loop)(in, out, n);
        }, [](double v) { return std::log2(v); }, mr);
}

Value log10(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::log10(c); }, mr);
    if (x.isScalar() && x.toScalar() < 0.0)
        return Value::complexScalar(std::log10(Complex(x.toScalar(), 0.0)), mr);
    if (anyNegative(x)) {
        Value cx = x; cx.promoteToComplex(mr);
        return unaryComplex(cx, [](const Complex &c) { return std::log10(c); }, mr);
    }
    return unaryRealArray(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(Log10Loop)(in, out, n);
        }, [](double v) { return std::log10(v); }, mr);
}

// reallog == log, but raises if any element is negative (MATLAB tells the
// user to switch to log for a complex result). Complex / scalar / non-DOUBLE
// stay on the reference path (preserves the per-element throw + type
// handling); a real DOUBLE array does one domain pass, then the SIMD log.
Value reallog(const Value &x, std::pmr::memory_resource *mr)
{
    auto scalarOp = [](double v) {
        if (v < 0.0)
            throw std::runtime_error("reallog produced complex result — use log(...) instead");
        return std::log(v);
    };
    if (x.isComplex() || x.isScalar() || x.type() != ValueType::DOUBLE)
        return unaryDouble(x, scalarOp, mr);

    const double     *in = x.doubleData();
    const std::size_t n  = x.numel();
    for (std::size_t i = 0; i < n; ++i)
        if (in[i] < 0.0)
            throw std::runtime_error("reallog produced complex result — use log(...) instead");

    Value r = createLike(x, ValueType::DOUBLE, mr);
    if (n == 0)
        return r;
    double *out = r.doubleDataMut();
    numkit::detail::parallel_for(n, numkit::detail::kTranscendentalThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(LogLoop)(in + s, out + s, e - s);
        });
    return r;
}

// True if any real element is < 0 (NaN compares false → stays real). sqrt of
// such an input goes complex; if ANY element is negative the WHOLE array is
// promoted to complex (MATLAB: sqrt([-1 4]) = [0+1i 2]).
static bool anyNegative(const Value &x)
{
    const std::size_t n = x.numel();
    for (std::size_t i = 0; i < n; ++i)
        if (x.elemAsDouble(i) < 0.0) return true;
    return false;
}

// sqrt: a negative input promotes to complex (MATLAB: sqrt(-1)==i). std::sqrt's
// complex branch matches MATLAB, so apply it to the promoted array directly.
Value sqrt(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sqrt(c); }, mr);
    if (anyNegative(x)) {
        Value cx = x; cx.promoteToComplex(mr);
        return unaryComplex(cx, [](const Complex &c) { return std::sqrt(c); }, mr);
    }
    return unaryRealArray(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(SqrtLoop)(in, out, n);
        }, [](double v) { return std::sqrt(v); }, mr);
}

// realsqrt: sqrt with a strict-nonnegative domain guard (now SIMD via SqrtLoop).
Value realsqrt(const Value &x, std::pmr::memory_resource *mr)
{
    auto scalarOp = [](double v) {
        if (v < 0.0)
            throw std::runtime_error("realsqrt produced complex result — use sqrt(...) instead");
        return std::sqrt(v);
    };
    if (x.isComplex() || x.isScalar() || x.type() != ValueType::DOUBLE)
        return unaryDouble(x, scalarOp, mr);

    const double     *in = x.doubleData();
    const std::size_t n  = x.numel();
    for (std::size_t i = 0; i < n; ++i)
        if (in[i] < 0.0)
            throw std::runtime_error("realsqrt produced complex result — use sqrt(...) instead");

    Value r = createLike(x, ValueType::DOUBLE, mr);
    if (n == 0)
        return r;
    double *out = r.doubleDataMut();
    numkit::detail::parallel_for(n, numkit::detail::kTranscendentalThreshold,
        [=](std::size_t s, std::size_t e) {
            HWY_DYNAMIC_DISPATCH(SqrtLoop)(in + s, out + s, e - s);
        });
    return r;
}

// pow2(y) == 2^y, SIMD via Exp2Loop (was scalar std::exp2). The 2-arg
// pow2(f, e) == f*2^e stays in exponents.cpp. Real-only, like MATLAB.
Value pow2(const Value &y, std::pmr::memory_resource *mr)
{
    return unaryRealArray(y, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(Exp2Loop)(in, out, n);
        }, [](double v) { return std::exp2(v); }, mr);
}

} // namespace numkit::builtin

#endif // HWY_ONCE
