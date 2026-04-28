// libs/builtin/src/backends/nan_reductions_simd.cpp
//
// Highway dynamic-dispatch single-pass nansum / nanmean kernels (P2).
// Reads the input ONCE; the NaN positions are masked to zero by
// `IfThenElse(IsNaN(v), Zero, v)` before adding to the lane-parallel
// accumulators. For the mean we accumulate a second per-lane counter
// (1.0 for non-NaN, 0.0 for NaN) and divide at the end.
//
// Old path was compactNonNan (full copy) → scalar sum → divide. New
// path is one streaming read, no scratch, no compaction, no second
// pass. 4× unrolled accumulators preserve the same ILP pattern as the
// existing binary-op SIMD loops.

#include "nan_reductions.hpp"

#include <cmath>
#include <cstddef>
#include <limits>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/nan_reductions_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

double NanSumScanLoop(const double *HWY_RESTRICT p, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto zero = hn::Zero(d);

    auto a0 = zero, a1 = zero, a2 = zero, a3 = zero;

    std::size_t i = 0;
    for (; i + 4 * N <= n; i += 4 * N) {
        const auto v0 = hn::LoadU(d, p + i + 0 * N);
        const auto v1 = hn::LoadU(d, p + i + 1 * N);
        const auto v2 = hn::LoadU(d, p + i + 2 * N);
        const auto v3 = hn::LoadU(d, p + i + 3 * N);
        a0 = hn::Add(a0, hn::IfThenElse(hn::IsNaN(v0), zero, v0));
        a1 = hn::Add(a1, hn::IfThenElse(hn::IsNaN(v1), zero, v1));
        a2 = hn::Add(a2, hn::IfThenElse(hn::IsNaN(v2), zero, v2));
        a3 = hn::Add(a3, hn::IfThenElse(hn::IsNaN(v3), zero, v3));
    }
    for (; i + N <= n; i += N) {
        const auto v = hn::LoadU(d, p + i);
        a0 = hn::Add(a0, hn::IfThenElse(hn::IsNaN(v), zero, v));
    }
    double sum = hn::ReduceSum(d, hn::Add(hn::Add(a0, a1), hn::Add(a2, a3)));
    for (; i < n; ++i)
        if (!std::isnan(p[i])) sum += p[i];
    return sum;
}

// Returns sum + count of non-NaN via out-pointers (avoids declaring a
// per-target struct return type that would break HWY_DYNAMIC_DISPATCH's
// shared function-pointer table). Counting via doubles (1.0 / 0.0 +
// ReduceSum) avoids the cross-type mask juggling that would be needed
// to keep counts in int64 — we never have N close to 2^53 so the
// precision loss is nil.
void NanSumCountScanLoop(const double *HWY_RESTRICT p, std::size_t n,
                         double *HWY_RESTRICT outSum,
                         double *HWY_RESTRICT outCount)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto zero = hn::Zero(d);
    const auto one  = hn::Set(d, 1.0);

    auto sum0 = zero, sum1 = zero, sum2 = zero, sum3 = zero;
    auto cnt0 = zero, cnt1 = zero, cnt2 = zero, cnt3 = zero;

    std::size_t i = 0;
    for (; i + 4 * N <= n; i += 4 * N) {
        const auto v0 = hn::LoadU(d, p + i + 0 * N);
        const auto v1 = hn::LoadU(d, p + i + 1 * N);
        const auto v2 = hn::LoadU(d, p + i + 2 * N);
        const auto v3 = hn::LoadU(d, p + i + 3 * N);
        const auto m0 = hn::IsNaN(v0);
        const auto m1 = hn::IsNaN(v1);
        const auto m2 = hn::IsNaN(v2);
        const auto m3 = hn::IsNaN(v3);
        sum0 = hn::Add(sum0, hn::IfThenElse(m0, zero, v0));
        sum1 = hn::Add(sum1, hn::IfThenElse(m1, zero, v1));
        sum2 = hn::Add(sum2, hn::IfThenElse(m2, zero, v2));
        sum3 = hn::Add(sum3, hn::IfThenElse(m3, zero, v3));
        cnt0 = hn::Add(cnt0, hn::IfThenElse(m0, zero, one));
        cnt1 = hn::Add(cnt1, hn::IfThenElse(m1, zero, one));
        cnt2 = hn::Add(cnt2, hn::IfThenElse(m2, zero, one));
        cnt3 = hn::Add(cnt3, hn::IfThenElse(m3, zero, one));
    }
    for (; i + N <= n; i += N) {
        const auto v = hn::LoadU(d, p + i);
        const auto m = hn::IsNaN(v);
        sum0 = hn::Add(sum0, hn::IfThenElse(m, zero, v));
        cnt0 = hn::Add(cnt0, hn::IfThenElse(m, zero, one));
    }
    double sum = hn::ReduceSum(d, hn::Add(hn::Add(sum0, sum1), hn::Add(sum2, sum3)));
    double cnt = hn::ReduceSum(d, hn::Add(hn::Add(cnt0, cnt1), hn::Add(cnt2, cnt3)));
    for (; i < n; ++i) {
        if (!std::isnan(p[i])) {
            sum += p[i];
            cnt += 1.0;
        }
    }
    *outSum = sum;
    *outCount = cnt;
}

// nanmax / nanmin: replace NaN lanes with the neutral element (-inf for
// max, +inf for min) so the SIMD reduction ignores them. Final scalar
// tail uses the same masking. Empty / all-NaN slices need a separate
// path (no valid lane to reduce); the dispatcher checks via a parallel
// "any non-NaN" accumulator and returns NaN if none seen.
//
// Returns the reduced value via outVal; sets *outAllNaN = 1.0 when no
// lane carried a non-NaN element (so caller can return NaN).
void NanMaxScanLoop(const double *HWY_RESTRICT p, std::size_t n,
                    double *HWY_RESTRICT outVal,
                    double *HWY_RESTRICT outAllNaN)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto neg_inf = hn::Set(d, -std::numeric_limits<double>::infinity());
    const auto zero    = hn::Zero(d);
    const auto one     = hn::Set(d, 1.0);

    auto m0 = neg_inf, m1 = neg_inf, m2 = neg_inf, m3 = neg_inf;
    auto cnt = zero;   // single accumulator is enough — only need >0 check

    std::size_t i = 0;
    for (; i + 4 * N <= n; i += 4 * N) {
        const auto v0 = hn::LoadU(d, p + i + 0 * N);
        const auto v1 = hn::LoadU(d, p + i + 1 * N);
        const auto v2 = hn::LoadU(d, p + i + 2 * N);
        const auto v3 = hn::LoadU(d, p + i + 3 * N);
        const auto k0 = hn::IsNaN(v0);
        const auto k1 = hn::IsNaN(v1);
        const auto k2 = hn::IsNaN(v2);
        const auto k3 = hn::IsNaN(v3);
        m0 = hn::Max(m0, hn::IfThenElse(k0, neg_inf, v0));
        m1 = hn::Max(m1, hn::IfThenElse(k1, neg_inf, v1));
        m2 = hn::Max(m2, hn::IfThenElse(k2, neg_inf, v2));
        m3 = hn::Max(m3, hn::IfThenElse(k3, neg_inf, v3));
        // Add 1 per non-NaN lane → cnt > 0 means at least one valid.
        cnt = hn::Add(cnt, hn::IfThenElse(k0, zero, one));
        cnt = hn::Add(cnt, hn::IfThenElse(k1, zero, one));
        cnt = hn::Add(cnt, hn::IfThenElse(k2, zero, one));
        cnt = hn::Add(cnt, hn::IfThenElse(k3, zero, one));
    }
    for (; i + N <= n; i += N) {
        const auto v = hn::LoadU(d, p + i);
        const auto k = hn::IsNaN(v);
        m0 = hn::Max(m0, hn::IfThenElse(k, neg_inf, v));
        cnt = hn::Add(cnt, hn::IfThenElse(k, zero, one));
    }
    double best = hn::ReduceMax(d, hn::Max(hn::Max(m0, m1), hn::Max(m2, m3)));
    double validCount = hn::ReduceSum(d, cnt);
    for (; i < n; ++i) {
        if (!std::isnan(p[i])) {
            if (p[i] > best) best = p[i];
            validCount += 1.0;
        }
    }
    *outVal = best;
    *outAllNaN = (validCount == 0.0) ? 1.0 : 0.0;
}

void NanMinScanLoop(const double *HWY_RESTRICT p, std::size_t n,
                    double *HWY_RESTRICT outVal,
                    double *HWY_RESTRICT outAllNaN)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto pos_inf = hn::Set(d, std::numeric_limits<double>::infinity());
    const auto zero    = hn::Zero(d);
    const auto one     = hn::Set(d, 1.0);

    auto m0 = pos_inf, m1 = pos_inf, m2 = pos_inf, m3 = pos_inf;
    auto cnt = zero;

    std::size_t i = 0;
    for (; i + 4 * N <= n; i += 4 * N) {
        const auto v0 = hn::LoadU(d, p + i + 0 * N);
        const auto v1 = hn::LoadU(d, p + i + 1 * N);
        const auto v2 = hn::LoadU(d, p + i + 2 * N);
        const auto v3 = hn::LoadU(d, p + i + 3 * N);
        const auto k0 = hn::IsNaN(v0);
        const auto k1 = hn::IsNaN(v1);
        const auto k2 = hn::IsNaN(v2);
        const auto k3 = hn::IsNaN(v3);
        m0 = hn::Min(m0, hn::IfThenElse(k0, pos_inf, v0));
        m1 = hn::Min(m1, hn::IfThenElse(k1, pos_inf, v1));
        m2 = hn::Min(m2, hn::IfThenElse(k2, pos_inf, v2));
        m3 = hn::Min(m3, hn::IfThenElse(k3, pos_inf, v3));
        cnt = hn::Add(cnt, hn::IfThenElse(k0, zero, one));
        cnt = hn::Add(cnt, hn::IfThenElse(k1, zero, one));
        cnt = hn::Add(cnt, hn::IfThenElse(k2, zero, one));
        cnt = hn::Add(cnt, hn::IfThenElse(k3, zero, one));
    }
    for (; i + N <= n; i += N) {
        const auto v = hn::LoadU(d, p + i);
        const auto k = hn::IsNaN(v);
        m0 = hn::Min(m0, hn::IfThenElse(k, pos_inf, v));
        cnt = hn::Add(cnt, hn::IfThenElse(k, zero, one));
    }
    double best = hn::ReduceMin(d, hn::Min(hn::Min(m0, m1), hn::Min(m2, m3)));
    double validCount = hn::ReduceSum(d, cnt);
    for (; i < n; ++i) {
        if (!std::isnan(p[i])) {
            if (p[i] < best) best = p[i];
            validCount += 1.0;
        }
    }
    *outVal = best;
    *outAllNaN = (validCount == 0.0) ? 1.0 : 0.0;
}

// Pass 2 of nan-aware variance: sum of (x - mean)^2 over non-NaN lanes.
// NaN lanes contribute 0 to the accumulator. Caller divides by the
// (count - {0,1}) it already computed in pass 1.
double NanSumSqDevScanLoop(const double *HWY_RESTRICT p, std::size_t n,
                           double mean)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto vmean = hn::Set(d, mean);
    const auto zero  = hn::Zero(d);
    auto a0 = zero, a1 = zero, a2 = zero, a3 = zero;

    std::size_t i = 0;
    for (; i + 4 * N <= n; i += 4 * N) {
        const auto v0 = hn::LoadU(d, p + i + 0 * N);
        const auto v1 = hn::LoadU(d, p + i + 1 * N);
        const auto v2 = hn::LoadU(d, p + i + 2 * N);
        const auto v3 = hn::LoadU(d, p + i + 3 * N);
        // Replace NaN values with mean → (mean - mean)^2 = 0 contribution
        const auto x0 = hn::IfThenElse(hn::IsNaN(v0), vmean, v0);
        const auto x1 = hn::IfThenElse(hn::IsNaN(v1), vmean, v1);
        const auto x2 = hn::IfThenElse(hn::IsNaN(v2), vmean, v2);
        const auto x3 = hn::IfThenElse(hn::IsNaN(v3), vmean, v3);
        const auto d0 = hn::Sub(x0, vmean);
        const auto d1 = hn::Sub(x1, vmean);
        const auto d2 = hn::Sub(x2, vmean);
        const auto d3 = hn::Sub(x3, vmean);
        a0 = hn::MulAdd(d0, d0, a0);
        a1 = hn::MulAdd(d1, d1, a1);
        a2 = hn::MulAdd(d2, d2, a2);
        a3 = hn::MulAdd(d3, d3, a3);
    }
    for (; i + N <= n; i += N) {
        const auto v = hn::LoadU(d, p + i);
        const auto x = hn::IfThenElse(hn::IsNaN(v), vmean, v);
        const auto dv = hn::Sub(x, vmean);
        a0 = hn::MulAdd(dv, dv, a0);
    }
    double ss = hn::ReduceSum(d, hn::Add(hn::Add(a0, a1), hn::Add(a2, a3)));
    for (; i < n; ++i) {
        if (!std::isnan(p[i])) {
            const double dv = p[i] - mean;
            ss += dv * dv;
        }
    }
    return ss;
}

} // namespace HWY_NAMESPACE
} // namespace numkit::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::builtin {

HWY_EXPORT(NanSumScanLoop);
HWY_EXPORT(NanSumCountScanLoop);
HWY_EXPORT(NanMaxScanLoop);
HWY_EXPORT(NanMinScanLoop);
HWY_EXPORT(NanSumSqDevScanLoop);

double nanSumScan(const double *p, std::size_t n)
{
    if (n == 0) return 0.0;
    return HWY_DYNAMIC_DISPATCH(NanSumScanLoop)(p, n);
}

NanSumCount nanSumCountScan(const double *p, std::size_t n)
{
    if (n == 0) return {0.0, 0};
    double sum = 0.0, cnt = 0.0;
    HWY_DYNAMIC_DISPATCH(NanSumCountScanLoop)(p, n, &sum, &cnt);
    return {sum, static_cast<std::size_t>(cnt)};
}

double nanMaxScan(const double *p, std::size_t n)
{
    if (n == 0) return std::nan("");
    double best = 0.0, allNaN = 0.0;
    HWY_DYNAMIC_DISPATCH(NanMaxScanLoop)(p, n, &best, &allNaN);
    return (allNaN != 0.0) ? std::nan("") : best;
}

double nanMinScan(const double *p, std::size_t n)
{
    if (n == 0) return std::nan("");
    double best = 0.0, allNaN = 0.0;
    HWY_DYNAMIC_DISPATCH(NanMinScanLoop)(p, n, &best, &allNaN);
    return (allNaN != 0.0) ? std::nan("") : best;
}

double nanVarianceTwoPass(const double *p, std::size_t n, int normFlag)
{
    if (n == 0) return std::nan("");
    const auto sc = nanSumCountScan(p, n);
    if (sc.count == 0) return std::nan("");
    if (sc.count == 1) return (normFlag == 1) ? 0.0 : std::nan("");
    const double mean  = sc.sum / static_cast<double>(sc.count);
    const double ss    = HWY_DYNAMIC_DISPATCH(NanSumSqDevScanLoop)(p, n, mean);
    const double denom = (normFlag == 1) ? static_cast<double>(sc.count)
                                         : static_cast<double>(sc.count - 1);
    return ss / denom;
}

} // namespace numkit::builtin
#endif // HWY_ONCE
