// libs/builtin/src/backends/MStdNanReductions_simd.cpp
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

#include "MStdNanReductions.hpp"

#include <cmath>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/MStdNanReductions_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::m::builtin {
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

} // namespace HWY_NAMESPACE
} // namespace numkit::m::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::m::builtin {

HWY_EXPORT(NanSumScanLoop);
HWY_EXPORT(NanSumCountScanLoop);

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

} // namespace numkit::m::builtin
#endif // HWY_ONCE
