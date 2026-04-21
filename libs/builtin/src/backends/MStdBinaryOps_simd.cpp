// libs/builtin/src/backends/MStdBinaryOps_simd.cpp
//
// Highway dynamic-dispatch inner loops for plus / minus / times /
// rdivide on real double arrays. One HWY_EXPORT / HWY_DYNAMIC_DISPATCH
// pair per op — the public entry points in the same detail namespace
// forward to the dispatcher. The surrounding public plus() / minus()
// / times() / rdivide() in MStdBinaryOps.cpp are unchanged; they
// call these loops only for the 2D same-shape DOUBLE fast path.

#include "BinaryOpsLoops.hpp"

#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/MStdBinaryOps_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::m::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void PlusLoop(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,
              double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N)
        hn::Store(hn::Add(hn::Load(d, a + i), hn::Load(d, b + i)), d, out + i);
    for (; i < n; ++i) out[i] = a[i] + b[i];
}

void MinusLoop(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,
               double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N)
        hn::Store(hn::Sub(hn::Load(d, a + i), hn::Load(d, b + i)), d, out + i);
    for (; i < n; ++i) out[i] = a[i] - b[i];
}

void TimesLoop(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,
               double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N)
        hn::Store(hn::Mul(hn::Load(d, a + i), hn::Load(d, b + i)), d, out + i);
    for (; i < n; ++i) out[i] = a[i] * b[i];
}

void RdivideLoop(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,
                 double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N)
        hn::Store(hn::Div(hn::Load(d, a + i), hn::Load(d, b + i)), d, out + i);
    for (; i < n; ++i) out[i] = a[i] / b[i];
}

} // namespace HWY_NAMESPACE
} // namespace numkit::m::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::m::builtin::detail {

HWY_EXPORT(PlusLoop);
HWY_EXPORT(MinusLoop);
HWY_EXPORT(TimesLoop);
HWY_EXPORT(RdivideLoop);

void plusLoop(const double *a, const double *b, double *out, std::size_t n)
{
    HWY_DYNAMIC_DISPATCH(PlusLoop)(a, b, out, n);
}

void minusLoop(const double *a, const double *b, double *out, std::size_t n)
{
    HWY_DYNAMIC_DISPATCH(MinusLoop)(a, b, out, n);
}

void timesLoop(const double *a, const double *b, double *out, std::size_t n)
{
    HWY_DYNAMIC_DISPATCH(TimesLoop)(a, b, out, n);
}

void rdivideLoop(const double *a, const double *b, double *out, std::size_t n)
{
    HWY_DYNAMIC_DISPATCH(RdivideLoop)(a, b, out, n);
}

} // namespace numkit::m::builtin::detail

#endif // HWY_ONCE
