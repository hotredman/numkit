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
    e.eval(std::string("nkfon = ") + expr + ";");          // fusion default-on
    e.setFusion(false);
    e.eval(std::string("nkfoff = ") + expr + ";");
    e.setFusion(true);
    return e.eval("isequaln(nkfon, nkfoff)").toBool();
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

// ---- affine: scale*x + offset (the fusedAffine kernel) ------------------

// a.*x + b and its commutations: fused == unfused, and numerically correct.
TEST_P(FusionParityTest, AffineMulAdd) {
    e.eval("x = reshape(linspace(-3, 4, 6000), 2000, 3); a = 2.5; b = -0.75;");
    EXPECT_TRUE(sameOnOff(e, "a .* x + b"));
    EXPECT_TRUE(sameOnOff(e, "x .* a + b"));
    EXPECT_TRUE(sameOnOff(e, "b + a .* x"));
    e.eval("y = a .* x + b;");
    EXPECT_NEAR(e.eval("y(1)").toScalar(), 2.5 * (-3.0) - 0.75, 1e-12);
}

// a.*x - b → fusedAffine(x, a, -b).
TEST_P(FusionParityTest, AffineMulSub) {
    e.eval("x = reshape(linspace(-3, 4, 6000), 2000, 3); a = 1.5; b = 0.25;");
    EXPECT_TRUE(sameOnOff(e, "a .* x - b"));
}

// `*` (scalar mtimes) spelling must fuse identically to `.*`.
TEST_P(FusionParityTest, AffineScalarStar) {
    e.eval("x = reshape(linspace(-1, 1, 6000), 2000, 3);");
    EXPECT_TRUE(sameOnOff(e, "2 * x + 1"));
    EXPECT_TRUE(sameOnOff(e, "x * 0.5 - 3"));
}

// NaN / Inf must flow through the affine kernel exactly as per-op (no clamp —
// this is the whole reason affine is a separate kernel from affine-clamp).
TEST_P(FusionParityTest, AffineNaNInf) {
    e.eval("x = [linspace(-1, 2, 5997)'; NaN; Inf; -Inf]; a = 3; b = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "a .* x + b"));
    e.eval("y = a .* x + b;");
    EXPECT_TRUE(e.eval("isnan(y(5998))").toBool());   // NaN preserved
    EXPECT_TRUE(e.eval("isinf(y(5999))").toBool());   // +Inf preserved
}

// Non-double / below-threshold inputs decline → fall back; still identical.
TEST_P(FusionParityTest, AffineNonDoubleFallsBack) {
    e.eval("x = uint8(reshape(mod(0:5999, 7), 2000, 3)); a = 2; b = 1;");
    EXPECT_TRUE(sameOnOff(e, "a .* x + b"));
}
TEST_P(FusionParityTest, AffineSmallFallsBack) {
    e.eval("x = linspace(-1, 2, 100); a = 2; b = 1;");
    EXPECT_TRUE(sameOnOff(e, "a .* x + b"));
}

// Two products (the axpby shape) must NOT mis-fire as affine — the affine
// matcher declines (b is not a pure leaf) → normal path, result still exact.
TEST_P(FusionParityTest, AffineTwoProductsFallBack) {
    e.eval("x = linspace(-1, 2, 6000); y = linspace(2, -1, 6000); a = 2; c = 3;");
    EXPECT_TRUE(sameOnOff(e, "a .* x + c .* y"));
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

// Same idea for the affine kernel `a.*x + b` (a BINARY_OP idiom, so it exercises
// the binary-op fusion hook), on BOTH backends — the VM is the one that matters
// for the XMAP pipeline. fusion-on collapses EMUL+ADD (a temp + two passes) into
// one streaming pass, so it must be clearly faster → proves it fired.
namespace {
void probeAffine(numkit::Engine::Backend backend, const char *tag) {
    numkit::StandardEngine e;
    e.setBackend(backend);
    e.eval("x = rand(3048*3816, 1); a = 2.5; b = -0.75;");
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval("y = a .* x + b;");  // warm
        const int iters = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval("y = a .* x + b;");
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] %s affine 11.6M: fusion-off %.2f ms, fusion-on %.2f ms "
                "(%.2fx)\n", tag, off, on, off / on);
    EXPECT_LT(on, off * 0.85);  // must be meaningfully faster → it fired
}
} // namespace

TEST(FusionFiringProbe, DISABLED_TreeWalkerAffineSpeedup) {
    probeAffine(numkit::Engine::Backend::TreeWalker, "TW");
}
TEST(FusionFiringProbe, DISABLED_VMAffineSpeedup) {
    probeAffine(numkit::Engine::Backend::VM, "VM");
}
