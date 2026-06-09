// toolboxes/builtin/src/math/exp_log/exp_log_portable.cpp
//
// Reference scalar implementations of exp / log. Compiled when
// NUMKIT_WITH_SIMD=OFF; the Highway-dispatched variant lives in
// exp_log_highway.cpp and matches this file bit-for-bit on complex
// inputs (SIMD only helps the real-vector fast path).

#include <numkit/builtin/math/exp_log/exponents.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>
#include <stdexcept>

namespace numkit::builtin {

namespace {

// Shared scaffolding: complex / scalar shortcuts, then either reuse
// `*hint` (uniquely-owned heap double of matching shape) or allocate
// a fresh result and apply ScalarOp element-wise. See the docblock
// on abs() in math/elementary/misc.hpp for the full hint contract.
template <typename ScalarOp, typename ComplexOp>
Value unaryRealDoubleHint(const Value &x, Value *hint, ScalarOp scalarOp, ComplexOp complexOp, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, complexOp, mr);
    if (x.isScalar())
        return Value::scalar(scalarOp(x.toScalar()), mr);

    if (hint && hint->isHeapDouble() && hint->heapRefCount() == 1
        && hint->dims() == x.dims()) {
        Value r = std::move(*hint);
        const double *in  = x.doubleData();
        double       *out = r.doubleDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            out[i] = scalarOp(in[i]);
        return r;
    }
    return unaryDouble(x, scalarOp, mr);
}

} // namespace

Value exp(const Value &x, Value *hint, std::pmr::memory_resource *mr)
{
    return unaryRealDoubleHint(x, hint, [](double v) { return std::exp(v); }, [](const Complex &c) { return std::exp(c); }, mr);
}

static bool anyNegative_p(const Value &x);  // defined below (shared with sqrt)

Value log(const Value &x, Value *hint, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::log(c); }, mr);
    if (x.isScalar() && x.toScalar() < 0)
        return Value::complexScalar(std::log(Complex(x.toScalar(), 0.0)), mr);
    // Real array with any negative element -> promote the whole array (MATLAB).
    if (anyNegative_p(x)) {
        Value cx = x; cx.promoteToComplex(mr);
        return unaryComplex(cx, [](const Complex &c) { return std::log(c); }, mr);
    }
    return unaryRealDoubleHint(x, hint, [](double v) { return std::log(v); }, [](const Complex &c) { return std::log(c); }, mr);
}

// expm1 / log1p / log2 — reference scalar path (the SIMD variants live in
// exp_log_highway.cpp). Real-only, so no complex branch.
Value expm1(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return std::expm1(v); }, mr);
}

// log1p of a real x as a (possibly) complex value, accurate per element:
//   x >= -1 : log1p(x)  (real; -Inf at x == -1)
//   x <  -1 : 1+x < 0 → log|1+x| + i·π   (MATLAB principal branch)
static inline Complex log1pRealToComplex_p(double x)
{
    constexpr double kPi = 3.141592653589793;
    if (x >= -1.0) return Complex(std::log1p(x), 0.0);
    return Complex(std::log(-(1.0 + x)), kPi);
}
static inline bool anyLessThanMinusOne_p(const Value &x)
{
    const std::size_t n = x.numel();
    for (std::size_t i = 0; i < n; ++i)
        if (x.elemAsDouble(i) < -1.0) return true;
    return false;
}

// log1p(x) = log(1+x). For x < -1 the argument 1+x is negative → MATLAB returns
// complex (log1p(-2) = i·π); any element < -1 promotes the whole real array.
// Complex input uses log(1+z). The real (x >= -1) path keeps accurate log1p.
Value log1p(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) {
            return std::log(Complex(1.0, 0.0) + c); }, mr);
    if (x.isScalar()) {
        const double v = x.toScalar();
        if (v < -1.0) return Value::complexScalar(log1pRealToComplex_p(v), mr);
        return Value::scalar(std::log1p(v), mr);
    }
    if (anyLessThanMinusOne_p(x)) {
        Value r = createLike(x, ValueType::COMPLEX, mr);
        Complex *out = r.complexDataMut();
        const std::size_t n = x.numel();
        for (std::size_t i = 0; i < n; ++i)
            out[i] = log1pRealToComplex_p(x.elemAsDouble(i));
        return r;
    }
    return unaryDouble(x, [](double v) { return std::log1p(v); }, mr);
}

Value log2(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::log(c) / std::log(2.0); }, mr);
    if (x.isScalar() && x.toScalar() < 0.0)
        return Value::complexScalar(std::log(Complex(x.toScalar(), 0.0)) / std::log(2.0), mr);
    if (anyNegative_p(x)) {
        Value cx = x; cx.promoteToComplex(mr);
        return unaryComplex(cx, [](const Complex &c) { return std::log(c) / std::log(2.0); }, mr);
    }
    return unaryDouble(x, [](double v) { return std::log2(v); }, mr);
}

Value log10(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::log10(c); }, mr);
    if (x.isScalar() && x.toScalar() < 0.0)
        return Value::complexScalar(std::log10(Complex(x.toScalar(), 0.0)), mr);
    if (anyNegative_p(x)) {
        Value cx = x; cx.promoteToComplex(mr);
        return unaryComplex(cx, [](const Complex &c) { return std::log10(c); }, mr);
    }
    return unaryDouble(x, [](double v) { return std::log10(v); }, mr);
}

Value reallog(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) {
        if (v < 0.0)
            throw std::runtime_error("reallog produced complex result — use log(...) instead");
        return std::log(v);
    }, mr);
}

// True if any real element is < 0 (NaN compares false → stays real). If ANY
// element is negative the whole array is promoted to complex (MATLAB).
static bool anyNegative_p(const Value &x)
{
    const std::size_t n = x.numel();
    for (std::size_t i = 0; i < n; ++i)
        if (x.elemAsDouble(i) < 0.0) return true;
    return false;
}

Value sqrt(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::sqrt(c); }, mr);
    if (anyNegative_p(x)) {
        Value cx = x; cx.promoteToComplex(mr);
        return unaryComplex(cx, [](const Complex &c) { return std::sqrt(c); }, mr);
    }
    return unaryDouble(x, [](double v) { return std::sqrt(v); }, mr);
}

Value realsqrt(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) {
        if (v < 0.0)
            throw std::runtime_error("realsqrt produced complex result — use sqrt(...) instead");
        return std::sqrt(v);
    }, mr);
}

Value pow2(const Value &y, std::pmr::memory_resource *mr)
{
    return unaryDouble(y, [](double v) { return std::exp2(v); }, mr);
}

} // namespace numkit::builtin
