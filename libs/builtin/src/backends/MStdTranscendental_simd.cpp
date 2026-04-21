// libs/builtin/src/backends/MStdTranscendental_simd.cpp
//
// Highway dynamic-dispatch sin / cos / exp / log. Each function gets
// its own HWY_EXPORT / HWY_DYNAMIC_DISPATCH pair; the HWY_NAMESPACE
// block up top holds the target-specific vector loops. Highway's
// hwy/contrib/math header provides the underlying Sin / Cos / Exp /
// Log primitives (SLEEF-derived polynomial approximations; ULP <= 4
// across all supported targets).
//
// The complex and scalar paths mirror MStdTranscendental_portable.cpp
// exactly — SIMD doesn't help there. Parity vs the scalar reference is
// verified in libs/builtin/tests/simd_parity_test.cpp.

#include <numkit/m/builtin/MStdMath.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "../MStdHelpers.hpp"

#include <cmath>
#include <complex>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/MStdTranscendental_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
#include <hwy/contrib/math/math-inl.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::m::builtin {
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

} // namespace HWY_NAMESPACE
} // namespace numkit::m::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::m::builtin {

HWY_EXPORT(SinLoop);
HWY_EXPORT(CosLoop);
HWY_EXPORT(ExpLoop);
HWY_EXPORT(LogLoop);

namespace {

// Shared shape for sin/cos/exp: delegate complex / scalar to the
// reference path, route real vectors through the dispatcher.
template <typename LoopDispatch, typename ScalarOp, typename ComplexOp>
MValue unaryRealDouble(Allocator &alloc, const MValue &x,
                       LoopDispatch loop, ScalarOp scalarOp, ComplexOp complexOp)
{
    if (x.isComplex()) {
        if (x.isScalar())
            return MValue::complexScalar(complexOp(x.toComplex()), &alloc);
        return unaryComplex(x, complexOp, &alloc);
    }
    if (x.isScalar())
        return MValue::scalar(scalarOp(x.toScalar()), &alloc);
    auto r = createLike(x, MType::DOUBLE, &alloc);
    loop(x.doubleData(), r.doubleDataMut(), x.numel());
    return r;
}

} // namespace

MValue sin(Allocator &alloc, const MValue &x)
{
    return unaryRealDouble(
        alloc, x,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(SinLoop)(in, out, n);
        },
        [](double v) { return std::sin(v); },
        [](const Complex &c) { return std::sin(c); });
}

MValue cos(Allocator &alloc, const MValue &x)
{
    return unaryRealDouble(
        alloc, x,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(CosLoop)(in, out, n);
        },
        [](double v) { return std::cos(v); },
        [](const Complex &c) { return std::cos(c); });
}

MValue exp(Allocator &alloc, const MValue &x)
{
    return unaryRealDouble(
        alloc, x,
        [](const double *in, double *out, std::size_t n) {
            HWY_DYNAMIC_DISPATCH(ExpLoop)(in, out, n);
        },
        [](double v) { return std::exp(v); },
        [](const Complex &c) { return std::exp(c); });
}

// log: MATLAB promotes a *scalar* negative input to complex (so
// log(-1) → i·π), but the element-wise path on a real vector just
// produces NaN for negatives — same as std::log. The SIMD Log()
// mirrors that behaviour.
MValue log(Allocator &alloc, const MValue &x)
{
    if (x.isComplex())
        return unaryComplex(x, [](const Complex &c) { return std::log(c); }, &alloc);
    if (x.isScalar() && x.toScalar() < 0)
        return MValue::complexScalar(std::log(Complex(x.toScalar(), 0.0)), &alloc);
    if (x.isScalar())
        return MValue::scalar(std::log(x.toScalar()), &alloc);
    auto r = createLike(x, MType::DOUBLE, &alloc);
    HWY_DYNAMIC_DISPATCH(LogLoop)(x.doubleData(), r.doubleDataMut(), x.numel());
    return r;
}

} // namespace numkit::m::builtin

#endif // HWY_ONCE
