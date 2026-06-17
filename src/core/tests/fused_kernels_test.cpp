// core/tests/fused_kernels_test.cpp
//
// Unit tests for the fused element-wise kernels (ops::fused*), in isolation —
// correctness vs a scalar reference (incl. NaN/Inf, output aliasing, tail), and
// a manual perf probe gating whether fusion is worth the VM integration.

#include <numkit/ops/fused/fused_kernels.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

using numkit::ops::fusedAffineClamp;

namespace {
double refAffineClamp(double x, double s, double t, double lo, double hi) {
    return std::fmax(lo, std::fmin(hi, s * x + t));
}
} // namespace

TEST(FusedKernelsTest, AffineClampMatchesScalar) {
    const std::size_t n = 1023;  // odd → exercises the SIMD tail
    std::vector<double> x(n), out(n);
    for (std::size_t i = 0; i < n; ++i) x[i] = 0.003 * (double)i - 1.0;
    const double s = 1.5, t = 0.2, lo = 0.0, hi = 1.0;
    fusedAffineClamp(x.data(), s, t, lo, hi, out.data(), n);
    for (std::size_t i = 0; i < n; ++i)
        EXPECT_DOUBLE_EQ(out[i], refAffineClamp(x[i], s, t, lo, hi)) << "i=" << i;
}

TEST(FusedKernelsTest, AffineClampNaNInf) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    std::vector<double> x = {-3.0, 0.0, 0.5, 2.0, nan, inf, -inf, 0.9, nan, -0.1};
    const std::size_t n = x.size();
    std::vector<double> out(n);
    const double s = 2.0, t = -0.5, lo = 0.0, hi = 1.0;
    fusedAffineClamp(x.data(), s, t, lo, hi, out.data(), n);
    for (std::size_t i = 0; i < n; ++i) {
        const double r = refAffineClamp(x[i], s, t, lo, hi);
        if (std::isnan(r)) EXPECT_TRUE(std::isnan(out[i])) << "i=" << i;
        else               EXPECT_DOUBLE_EQ(out[i], r) << "i=" << i;
    }
}

TEST(FusedKernelsTest, AffineClampOutAliasesInput) {
    const std::size_t n = 777;
    std::vector<double> x(n), x0(n);
    for (std::size_t i = 0; i < n; ++i) x[i] = x0[i] = 0.01 * (double)i - 2.0;
    const double s = 0.7, t = 0.1, lo = -0.5, hi = 0.5;
    fusedAffineClamp(x.data(), s, t, lo, hi, x.data(), n);  // out == x
    for (std::size_t i = 0; i < n; ++i)
        EXPECT_DOUBLE_EQ(x[i], refAffineClamp(x0[i], s, t, lo, hi)) << "i=" << i;
}

// Manual gate (run with --gtest_also_run_disabled_tests): fused affine-clamp
// over 11.6 MP, single-thread. Compare to the real per-op VM chain
// max(0,min(1,x.*1.5+0.2)) = ~71 ms. Need a convincing margin (~>=1.8x) to
// justify the VM fusion integration.
TEST(FusedKernelsTest, DISABLED_PerfAffineClamp11M) {
    const std::size_t n = 3048ull * 3816ull;
    std::vector<double> x(n), out(n);
    for (std::size_t i = 0; i < n; ++i) x[i] = (double)(i % 1000) / 700.0 - 0.3;
    fusedAffineClamp(x.data(), 1.5, 0.2, 0.0, 1.0, out.data(), n);  // warm
    const int iters = 30;
    auto a = std::chrono::steady_clock::now();
    for (int it = 0; it < iters; ++it)
        fusedAffineClamp(x.data(), 1.5, 0.2, 0.0, 1.0, out.data(), n);
    auto b = std::chrono::steady_clock::now();
    std::printf("[fused] affine-clamp 11.6M single-thread: %.2f ms/call  "
                "(per-op VM baseline ~71 ms)\n",
                std::chrono::duration<double, std::milli>(b - a).count() / iters);
    SUCCEED();
}
