// libs/builtin/src/math/exp_log/exp_log_portable.cpp
//
// Reference scalar implementations of exp / log. Compiled when
// NUMKIT_WITH_SIMD=OFF; the Highway-dispatched variant lives in
// exp_log_highway.cpp and matches this file bit-for-bit on complex
// inputs (SIMD only helps the real-vector fast path).

#include <numkit/builtin/math/exp_log/exponents.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

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

Value log(const Value &x, Value *hint, std::pmr::memory_resource *mr)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::log(c); }, mr);
    if (x.isScalar() && x.toScalar() < 0)
        return Value::complexScalar(std::log(Complex(x.toScalar(), 0.0)), mr);
    return unaryRealDoubleHint(x, hint, [](double v) { return std::log(v); }, [](const Complex &c) { return std::log(c); }, mr);
}

// expm1 / log1p / log2 — reference scalar path (the SIMD variants live in
// exp_log_highway.cpp). Real-only, so no complex branch.
Value expm1(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return std::expm1(v); }, mr);
}

Value log1p(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return std::log1p(v); }, mr);
}

Value log2(const Value &x, std::pmr::memory_resource *mr)
{
    return unaryDouble(x, [](double v) { return std::log2(v); }, mr);
}

Value log10(const Value &x, std::pmr::memory_resource *mr)
{
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

} // namespace numkit::builtin
