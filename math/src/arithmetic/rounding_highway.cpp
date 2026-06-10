// toolboxes/builtin/src/math/arithmetic/rounding_highway.cpp
//
// Highway dynamic-dispatch ceil / floor / round / fix on doubles.
// Highway provides direct primitives for Ceil, Floor, Trunc; for
// MATLAB's round (round-half-away-from-zero) we use the
// CopySign(0.5, v) + Trunc trick, since Highway's `Round` is
// round-to-nearest-even.

#include "rounding.hpp"

#include <cmath>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "arithmetic/rounding_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::math::detail {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void CeilLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Ceil(v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::ceil(in[i]);
}

void FloorLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Floor(v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::floor(in[i]);
}

void FixLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    // fix() = std::trunc — round toward zero.
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        hn::StoreU(hn::Trunc(v), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::trunc(in[i]);
}

void RoundLoop(const double *HWY_RESTRICT in, double *HWY_RESTRICT out, std::size_t n)
{
    // MATLAB round() = round-half-away-from-zero (ties move away from
    // zero: round(0.5)=1, round(-0.5)=-1). Highway's Round is RTNE.
    // Compute as Trunc(v + CopySign(0.5, v)) to match MATLAB.
    // NaN propagates because CopySign(NaN, _)=NaN and Trunc(NaN)=NaN.
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto half = hn::Set(d, 0.5);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        auto h = hn::CopySign(half, v);
        hn::StoreU(hn::Trunc(hn::Add(v, h)), d, out + i);
    }
    for (; i < n; ++i) out[i] = std::round(in[i]);
}

} // namespace HWY_NAMESPACE
} // namespace numkit::math::detail
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::math::detail {

HWY_EXPORT(CeilLoop);
HWY_EXPORT(FloorLoop);
HWY_EXPORT(RoundLoop);
HWY_EXPORT(FixLoop);

void doubleCeilLoop(const double *in, double *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(CeilLoop)(in, out, n);
}
void doubleFloorLoop(const double *in, double *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(FloorLoop)(in, out, n);
}
void doubleRoundLoop(const double *in, double *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(RoundLoop)(in, out, n);
}
void doubleFixLoop(const double *in, double *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(FixLoop)(in, out, n);
}

} // namespace numkit::math::detail

#endif // HWY_ONCE
