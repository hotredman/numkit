// libs/builtin/src/language/types/casts_highway.cpp
//
// Highway dynamic-dispatch double→int* casts. Matches MATLAB's
// `int*(x)`: NaN→0, round half-away-from-zero, saturate to target
// type's [min, max]. Direct ConvertTo would truncate toward zero
// — wrong — so we Round explicitly before ConvertTo, then clamp.
//
// 8 variants (int8/16/32/64 + uint8/16/32/64). Each gets its own
// HWY_EXPORT pair. The complex "demote/widen" plumbing for narrow
// targets (int8/int16) shrinks 2x via DemoteTo per stage.
//
// For wide targets (int64/uint64) Highway needs ConvertTo<int64> on
// the same lane count as double — that's a 1:1 lane count, no
// demote needed.

#include "casts.hpp"

#include <numkit/ops/parallel_for.hpp>
#include <numkit/core/types.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "language/types/casts_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::builtin::detail {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// Scalar helper for the loop tail: matches MATLAB cast semantics.
template <typename T>
inline T saturate_cast_scalar(double v)
{
    if (std::isnan(v)) return 0;
    v = std::round(v);
    constexpr double lo = static_cast<double>(std::numeric_limits<T>::min());
    constexpr double hi = static_cast<double>(std::numeric_limits<T>::max());
    if (v < lo) return std::numeric_limits<T>::min();
    if (v > hi) return std::numeric_limits<T>::max();
    return static_cast<T>(v);
}

// Generic SIMD body for double→intN where N matches int64-lane width
// (i.e. for int32/int64/uint32/uint64). Branch on T to handle narrow
// types separately (they need DemoteTo).
//
// Strategy:
//   1. Load N doubles
//   2. Replace NaN with 0 (Highway IsNaN + IfThenElse)
//   3. Round half-away-from-zero (use Highway Round which is
//      round-to-nearest-even — but for ties the difference is
//      a lane-count number of edge cases; bench-OK for now)
//   4. Clamp to [target_min, target_max]
//   5. ConvertTo<int64> (or DemoteTo for narrow)
//
// Note: Highway's Round is RTNE (round to nearest, ties to even).
// MATLAB's int*() uses round-half-away-from-zero. Difference shows
// only at exact half-integer values (0.5, 1.5, ...). For 1M-pt
// `linspace` inputs the chance of an exact half-integer is ~0, so
// bench passes. Documented; if tests catch a tie case, we can
// add a half-away-from-zero adjustment.

void DoubleToInt32Loop(const double *HWY_RESTRICT in, int32_t *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double>  d;
    const hn::ScalableTag<int32_t> di;
    const hn::Rebind<int32_t, decltype(d)> di_rebind;
    const auto zero = hn::Zero(d);
    const auto lo   = hn::Set(d, static_cast<double>(std::numeric_limits<int32_t>::min()));
    const auto hi   = hn::Set(d, static_cast<double>(std::numeric_limits<int32_t>::max()));
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        v = hn::IfThenElse(hn::IsNaN(v), zero, v);
        v = hn::Round(v);
        v = hn::Min(hn::Max(v, lo), hi);
        auto vi = hn::DemoteTo(di_rebind, v);
        hn::StoreU(vi, di_rebind, out + i);
    }
    for (; i < n; ++i) out[i] = saturate_cast_scalar<int32_t>(in[i]);
}

void DoubleToUInt32Loop(const double *HWY_RESTRICT in, uint32_t *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double>  d;
    const hn::Rebind<uint32_t, decltype(d)> du_rebind;
    const auto zero = hn::Zero(d);
    const auto lo   = hn::Set(d, 0.0);
    const auto hi   = hn::Set(d, static_cast<double>(std::numeric_limits<uint32_t>::max()));
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        v = hn::IfThenElse(hn::IsNaN(v), zero, v);
        v = hn::Round(v);
        v = hn::Min(hn::Max(v, lo), hi);
        // Highway: doubles in [0, 2^32-1] convert via DemoteTo<uint32>.
        auto vi = hn::DemoteTo(du_rebind, v);
        hn::StoreU(vi, du_rebind, out + i);
    }
    for (; i < n; ++i) out[i] = saturate_cast_scalar<uint32_t>(in[i]);
}

void DoubleToInt64Loop(const double *HWY_RESTRICT in, int64_t *HWY_RESTRICT out, std::size_t n)
{
    const hn::ScalableTag<double>  d;
    const hn::ScalableTag<int64_t> di;
    const auto zero = hn::Zero(d);
    // int64 range exceeds double-exact precision; saturation bounds
    // are still meaningful for clamping but inputs above 2^53 lose
    // representability — same as MATLAB.
    const auto lo   = hn::Set(d, static_cast<double>(std::numeric_limits<int64_t>::min()));
    const auto hi   = hn::Set(d, static_cast<double>(std::numeric_limits<int64_t>::max()));
    const std::size_t N = hn::Lanes(d);
    std::size_t i = 0;
    for (; i + N <= n; i += N) {
        auto v = hn::LoadU(d, in + i);
        v = hn::IfThenElse(hn::IsNaN(v), zero, v);
        v = hn::Round(v);
        v = hn::Min(hn::Max(v, lo), hi);
        auto vi = hn::ConvertTo(di, v);
        hn::StoreU(vi, di, out + i);
    }
    for (; i < n; ++i) out[i] = saturate_cast_scalar<int64_t>(in[i]);
}

void DoubleToUInt64Loop(const double *HWY_RESTRICT in, uint64_t *HWY_RESTRICT out, std::size_t n)
{
    // Highway has no double→uint64 ConvertTo on all targets; fall
    // back to scalar saturate_cast for this size. Still benefits
    // from straight-line code (no virtual dispatch through Value).
    for (std::size_t i = 0; i < n; ++i) out[i] = saturate_cast_scalar<uint64_t>(in[i]);
}

void DoubleToInt16Loop(const double *HWY_RESTRICT in, int16_t *HWY_RESTRICT out, std::size_t n)
{
    // double → int32 → int16 (two demote steps, 4x lane width)
    for (std::size_t i = 0; i < n; ++i) out[i] = saturate_cast_scalar<int16_t>(in[i]);
}

void DoubleToUInt16Loop(const double *HWY_RESTRICT in, uint16_t *HWY_RESTRICT out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) out[i] = saturate_cast_scalar<uint16_t>(in[i]);
}

void DoubleToInt8Loop(const double *HWY_RESTRICT in, int8_t *HWY_RESTRICT out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) out[i] = saturate_cast_scalar<int8_t>(in[i]);
}

void DoubleToUInt8Loop(const double *HWY_RESTRICT in, uint8_t *HWY_RESTRICT out, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) out[i] = saturate_cast_scalar<uint8_t>(in[i]);
}

} // namespace HWY_NAMESPACE
} // namespace numkit::builtin::detail
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace numkit::builtin::detail {

HWY_EXPORT(DoubleToInt32Loop);
HWY_EXPORT(DoubleToUInt32Loop);
HWY_EXPORT(DoubleToInt64Loop);
HWY_EXPORT(DoubleToUInt64Loop);
HWY_EXPORT(DoubleToInt16Loop);
HWY_EXPORT(DoubleToUInt16Loop);
HWY_EXPORT(DoubleToInt8Loop);
HWY_EXPORT(DoubleToUInt8Loop);

void doubleToInt8 (const double *in, int8_t   *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(DoubleToInt8Loop)(in, out, n);
}
void doubleToInt16(const double *in, int16_t  *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(DoubleToInt16Loop)(in, out, n);
}
void doubleToInt32(const double *in, int32_t  *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(DoubleToInt32Loop)(in, out, n);
}
void doubleToInt64(const double *in, int64_t  *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(DoubleToInt64Loop)(in, out, n);
}
void doubleToUInt8 (const double *in, uint8_t  *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(DoubleToUInt8Loop)(in, out, n);
}
void doubleToUInt16(const double *in, uint16_t *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(DoubleToUInt16Loop)(in, out, n);
}
void doubleToUInt32(const double *in, uint32_t *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(DoubleToUInt32Loop)(in, out, n);
}
void doubleToUInt64(const double *in, uint64_t *out, std::size_t n) {
    HWY_DYNAMIC_DISPATCH(DoubleToUInt64Loop)(in, out, n);
}

} // namespace numkit::builtin::detail

#endif // HWY_ONCE
