// core/tests/fusion_parity_test.cpp
//
// Stage 2 of VM element-wise fusion: parity harness. For each registered idiom
// it asserts fused (fusion on) == unfused (fusion off) bit-for-bit, on a
// StandardEngine, across both backends and across the cases that should fall
// back (wrong type, small N) and the NaN/Inf edges. The TreeWalker backend
// exercises the fused path today; the VM backend is unaffected until Stage 3
// adds FUSE_EWISE — running it here keeps the harness ready and confirms the VM
// is unchanged.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <string>

namespace {

// isequaln: exact, NaN-aware (NaN==NaN) — the right fused-vs-unfused comparator.
bool sameOnOff(numkit::Engine &e, const char *expr) {
    e.eval(std::string("__fon = ") + expr + ";");          // fusion default-on
    e.setFusion(false);
    e.eval(std::string("__foff = ") + expr + ";");
    e.setFusion(true);
    return e.eval("isequaln(__fon, __foff)").toBool();
}

} // namespace

class FusionParityTest
    : public ::testing::TestWithParam<numkit::Engine::Backend> {
protected:
    numkit::StandardEngine e;
    void SetUp() override { e.setBackend(GetParam()); }
};

// The standard engine registers fusion rules → fusion is live (on TreeWalker).
TEST_P(FusionParityTest, FusionIsRegistered) {
    EXPECT_TRUE(e.fusionEnabled());
}

// clamp over values below/inside/above [0,1] — fused == unfused, and correct.
TEST_P(FusionParityTest, ClampRange) {
    e.eval("x = reshape(linspace(-2, 3, 6000), 2000, 3);");
    EXPECT_TRUE(sameOnOff(e, "max(0, min(1, x))"));
    e.eval("y = max(0, min(1, x));");
    EXPECT_GE(e.eval("min(y(:))").toScalar(), 0.0);
    EXPECT_LE(e.eval("max(y(:))").toScalar(), 1.0);
}

// NaN / Inf must clamp identically fused vs unfused (the kernel's fmin/fmax
// NaN semantics must match the per-op min/max).
TEST_P(FusionParityTest, ClampNaNInf) {
    e.eval("x = [linspace(-1, 2, 5997)'; NaN; Inf; -Inf];");
    EXPECT_TRUE(sameOnOff(e, "max(0, min(1, x))"));
}

// Bounds as variables (still pure leaves) must fuse + match.
TEST_P(FusionParityTest, ClampVariableBounds) {
    e.eval("x = reshape(linspace(-2, 3, 6000), 2000, 3); lo = 0.2; hi = 0.8;");
    EXPECT_TRUE(sameOnOff(e, "max(lo, min(hi, x))"));
}

// Non-double input → kernel declines → falls back; result still identical.
TEST_P(FusionParityTest, NonDoubleFallsBack) {
    e.eval("x = uint8(reshape(mod(0:5999, 4), 2000, 3));");
    EXPECT_TRUE(sameOnOff(e, "max(0, min(1, x))"));
}

// Below the fusion size threshold → declines → falls back; identical.
TEST_P(FusionParityTest, SmallArrayFallsBack) {
    e.eval("x = linspace(-1, 2, 100);");
    EXPECT_TRUE(sameOnOff(e, "max(0, min(1, x))"));
}

INSTANTIATE_TEST_SUITE_P(Backends, FusionParityTest,
                         ::testing::Values(numkit::Engine::Backend::TreeWalker,
                                           numkit::Engine::Backend::VM));

// Manual probe (--gtest_also_run_disabled_tests): confirms the TreeWalker fused
// path actually FIRES (parity alone can't — a silent no-match also passes). On
// an 11.6 MP clamp, fusion-on should be clearly faster than fusion-off.
TEST(FusionFiringProbe, DISABLED_TreeWalkerClampSpeedup) {
    numkit::StandardEngine e;
    e.setBackend(numkit::Engine::Backend::TreeWalker);
    e.eval("x = rand(3048*3816, 1);");
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval("y = max(0, min(1, x));");  // warm
        const int iters = 20;
        auto a = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval("y = max(0, min(1, x));");
        auto b = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(b - a).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] TW clamp 11.6M: fusion-off %.2f ms, fusion-on %.2f ms "
                "(%.2fx)\n", off, on, off / on);
    EXPECT_LT(on, off * 0.85);  // must be meaningfully faster → it fired
}
