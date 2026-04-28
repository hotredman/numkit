// libs/builtin/src/math/elementary/backends/var_reduction_simd.cpp
//
// Highway dynamic-dispatch two-pass variance kernel (P5). Two scans:
//   pass 1: lane-parallel sum → mean
//   pass 2: lane-parallel sum of (x - mean)^2 via FMA (Highway MulAdd)
//          → variance after dividing by n or n-1.
// 4× unrolled accumulators per pass keep enough independent ops in
// flight to hide load-to-use latency on the wide pipelines.

#include "var_reduction.hpp"

#include <cmath>
#include <cstddef>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/var_reduction_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

double SumScanLoop(const double *HWY_RESTRICT p, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    auto a0 = hn::Zero(d), a1 = hn::Zero(d), a2 = hn::Zero(d), a3 = hn::Zero(d);

    std::size_t i = 0;
    for (; i + 4 * N <= n; i += 4 * N) {
        a0 = hn::Add(a0, hn::LoadU(d, p + i + 0 * N));
        a1 = hn::Add(a1, hn::LoadU(d, p + i + 1 * N));
        a2 = hn::Add(a2, hn::LoadU(d, p + i + 2 * N));
        a3 = hn::Add(a3, hn::LoadU(d, p + i + 3 * N));
    }
    for (; i + N <= n; i += N)
        a0 = hn::Add(a0, hn::LoadU(d, p + i));
    double s = hn::ReduceSum(d, hn::Add(hn::Add(a0, a1), hn::Add(a2, a3)));
    for (; i < n; ++i) s += p[i];
    return s;
}

// In-place accumulate: dst += src. 4× unrolled for ILP, regular
// cached stores (caller reads dst again on the next column-pass iter).
void AddIntoLoop(double *HWY_RESTRICT dst, const double *HWY_RESTRICT src,
                 std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);

    std::size_t i = 0;
    for (; i + 4 * N <= n; i += 4 * N) {
        const auto d0 = hn::Add(hn::LoadU(d, dst + i + 0 * N),
                                hn::LoadU(d, src + i + 0 * N));
        const auto d1 = hn::Add(hn::LoadU(d, dst + i + 1 * N),
                                hn::LoadU(d, src + i + 1 * N));
        const auto d2 = hn::Add(hn::LoadU(d, dst + i + 2 * N),
                                hn::LoadU(d, src + i + 2 * N));
        const auto d3 = hn::Add(hn::LoadU(d, dst + i + 3 * N),
                                hn::LoadU(d, src + i + 3 * N));
        hn::StoreU(d0, d, dst + i + 0 * N);
        hn::StoreU(d1, d, dst + i + 1 * N);
        hn::StoreU(d2, d, dst + i + 2 * N);
        hn::StoreU(d3, d, dst + i + 3 * N);
    }
    for (; i + N <= n; i += N)
        hn::StoreU(hn::Add(hn::LoadU(d, dst + i), hn::LoadU(d, src + i)),
                   d, dst + i);
    for (; i < n; ++i) dst[i] += src[i];
}

double SumSquaredDeviationsScanLoop(const double *HWY_RESTRICT p, std::size_t n,
                                    double mean)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto vmean = hn::Set(d, mean);
    auto a0 = hn::Zero(d), a1 = hn::Zero(d), a2 = hn::Zero(d), a3 = hn::Zero(d);

    std::size_t i = 0;
    for (; i + 4 * N <= n; i += 4 * N) {
        const auto v0 = hn::Sub(hn::LoadU(d, p + i + 0 * N), vmean);
        const auto v1 = hn::Sub(hn::LoadU(d, p + i + 1 * N), vmean);
        const auto v2 = hn::Sub(hn::LoadU(d, p + i + 2 * N), vmean);
        const auto v3 = hn::Sub(hn::LoadU(d, p + i + 3 * N), vmean);
        // MulAdd(a, b, c) = a*b + c with one rounding (FMA when available).
        a0 = hn::MulAdd(v0, v0, a0);
        a1 = hn::MulAdd(v1, v1, a1);
        a2 = hn::MulAdd(v2, v2, a2);
        a3 = hn::MulAdd(v3, v3, a3);
    }
    for (; i + N <= n; i += N) {
        const auto v = hn::Sub(hn::LoadU(d, p + i), vmean);
        a0 = hn::MulAdd(v, v, a0);
    }
    double ss = hn::ReduceSum(d, hn::Add(hn::Add(a0, a1), hn::Add(a2, a3)));
    for (; i < n; ++i) {
        const double dv = p[i] - mean;
        ss += dv * dv;
    }
    return ss;
}

} // namespace HWY_NAMESPACE
} // namespace numkit::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::builtin {

HWY_EXPORT(SumScanLoop);
HWY_EXPORT(SumSquaredDeviationsScanLoop);
HWY_EXPORT(AddIntoLoop);

double sumScan(const double *p, std::size_t n)
{
    if (n == 0) return 0.0;
    return HWY_DYNAMIC_DISPATCH(SumScanLoop)(p, n);
}

double sumSquaredDeviationsScan(const double *p, std::size_t n, double mean)
{
    if (n == 0) return 0.0;
    return HWY_DYNAMIC_DISPATCH(SumSquaredDeviationsScanLoop)(p, n, mean);
}

void addInto(double *dst, const double *src, std::size_t n)
{
    if (n == 0) return;
    HWY_DYNAMIC_DISPATCH(AddIntoLoop)(dst, src, n);
}

double varianceTwoPass(const double *p, std::size_t n, int normFlag)
{
    if (n == 0) return std::nan("");
    if (n == 1) return (normFlag == 1) ? 0.0 : std::nan("");
    const double mean  = sumScan(p, n) / static_cast<double>(n);
    const double ss    = sumSquaredDeviationsScan(p, n, mean);
    const double denom = (normFlag == 1) ? static_cast<double>(n)
                                         : static_cast<double>(n - 1);
    return ss / denom;
}

} // namespace numkit::builtin
#endif // HWY_ONCE
