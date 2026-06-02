// libs/builtin/src/math/exp_log/exp_log_highway.cpp
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
// verified in libs/builtin/tests/simd_parity_test.cpp.

#include <numkit/builtin/math/exp_log/exponents.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/parallel_for.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>
#include <cstddef>
#include <stdexcept>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "math/exp_log/exp_log_highway.cpp"
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
Value log(const Value &x, Value *hint, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::log(c); }, mr);
    if (x.isScalar() && x.toScalar() < 0)
        return Value::complexScalar(std::log(Complex(x.toScalar(), 0.0)), mr);
    if (x.isScalar())
        return Value::scalar(std::log(x.toScalar()), mr);

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

Value log1p(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealArray(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(Log1pLoop)(in, out, n);
        }, [](double v) { return std::log1p(v); }, mr);
}

Value log2(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryRealArray(x, [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(Log2Loop)(in, out, n);
        }, [](double v) { return std::log2(v); }, mr);
}

Value log10(const Value &x, std::pmr::memory_resource *mr)
{
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

} // namespace numkit::builtin

#endif // HWY_ONCE
