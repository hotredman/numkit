// core/tests/decimate_test.cpp
//
// Engine-side line decimation (decimate.hpp) — must agree with the frontend
// ide/src/components/v3/decimate.js so Phase 2 (engine tiles) renders the
// same as Phase 1 (JS).

#include <gtest/gtest.h>

#include <numkit/core/decimate.hpp>

#include <vector>
#include <cmath>

using numkit::decimateM4;
using numkit::decimateLTTB;
using numkit::decimateSeries;
using numkit::decimVisibleRange;
using numkit::DecimAlgo;

namespace {
std::vector<double> ramp(std::size_t n) {
    std::vector<double> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<double>(i);
    return v;
}
}  // namespace

TEST(DecimateVisibleRange, PadsOneEachSideAndClamps) {
    auto x = ramp(10);   // 0..9
    std::size_t i0, i1;
    decimVisibleRange(x.data(), x.size(), 3.0, 6.0, i0, i1);
    EXPECT_LE(x[i0], 3.0);          // padded left
    EXPECT_GE(x[i1 - 1], 6.0);      // padded right
    decimVisibleRange(x.data(), x.size(), -100.0, 100.0, i0, i1);
    EXPECT_EQ(i0, 0u);
    EXPECT_EQ(i1, 10u);
}

TEST(DecimateM4, PreservesGlobalMinMax) {
    const std::size_t N = 1000;
    auto x = ramp(N);
    std::vector<double> y(N);
    for (std::size_t i = 0; i < N; ++i) y[i] = std::sin(i / 7.0);
    y[500] = 99.0;    // spike
    y[123] = -99.0;   // dip
    auto out = decimateM4(x.data(), y.data(), N, 0.0, N - 1.0, 50);
    double mn = out.y[0], mx = out.y[0];
    for (double v : out.y) { mn = std::min(mn, v); mx = std::max(mx, v); }
    EXPECT_DOUBLE_EQ(mx, 99.0);
    EXPECT_DOUBLE_EQ(mn, -99.0);
}

TEST(DecimateM4, BoundedPointsAndAscendingX) {
    const std::size_t N = 100000;
    const int W = 800;
    auto x = ramp(N);
    std::vector<double> y(N);
    for (std::size_t i = 0; i < N; ++i) y[i] = std::sin(i / 50.0);
    auto out = decimateM4(x.data(), y.data(), N, 0.0, N - 1.0, W);
    EXPECT_LE(out.x.size(), static_cast<std::size_t>(4 * W));
    EXPECT_GT(out.x.size(), 0u);
    for (std::size_t i = 1; i < out.x.size(); ++i)
        EXPECT_GE(out.x[i], out.x[i - 1]);
}

TEST(DecimateM4, RestrictsToVisibleRange) {
    const std::size_t N = 1000;
    auto x = ramp(N), y = ramp(N);
    auto out = decimateM4(x.data(), y.data(), N, 100.0, 200.0, 50);
    EXPECT_GE(out.x.front(), 99.0);
    EXPECT_LE(out.x.back(), 201.0);
}

TEST(DecimateLTTB, KeepsEndpointsAndThresholdCount) {
    const std::size_t N = 1000;
    auto x = ramp(N);
    std::vector<double> y(N);
    for (std::size_t i = 0; i < N; ++i) y[i] = std::sin(i / 11.0);
    auto out = decimateLTTB(x.data(), y.data(), N, 0.0, N - 1.0, 100);
    EXPECT_DOUBLE_EQ(out.x.front(), 0.0);
    EXPECT_DOUBLE_EQ(out.x.back(), N - 1.0);
    EXPECT_EQ(out.x.size(), 100u);
}

TEST(DecimateSeries, RawWhenSeriesFitsWidth) {
    std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4};
    auto out = decimateSeries(x.data(), y.data(), x.size(), 0.0, 4.0, 100, DecimAlgo::M4);
    EXPECT_EQ(out.y.size(), 5u);
    EXPECT_DOUBLE_EQ(out.y[2], 2.0);
}

TEST(DecimateSeries, DecimatesWhenLarge) {
    const std::size_t N = 100000;
    const int W = 800;
    auto x = ramp(N);
    std::vector<double> y(N);
    for (std::size_t i = 0; i < N; ++i) y[i] = std::sin(i / 50.0);
    auto out = decimateSeries(x.data(), y.data(), N, 0.0, N - 1.0, W, DecimAlgo::M4);
    EXPECT_LE(out.x.size(), static_cast<std::size_t>(4 * W));
    EXPECT_GT(out.x.size(), static_cast<std::size_t>(W));   // clearly downsampled, not raw
}

TEST(DecimateSeries, NoneReturnsRaw) {
    const std::size_t N = 10000;
    auto x = ramp(N), y = ramp(N);
    auto out = decimateSeries(x.data(), y.data(), N, 0.0, N - 1.0, 100, DecimAlgo::None);
    EXPECT_EQ(out.x.size(), N);
}
