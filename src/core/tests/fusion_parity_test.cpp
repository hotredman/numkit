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
#include <cmath>
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

// Two products (`a.*x + c.*y`): affine_add declines (its offset `c.*y` is not a
// pure leaf) and axpby_add picks it up — disjoint matchers, so exactly one rule
// fires and either way the result is bit-exact.
TEST_P(FusionParityTest, TwoProductsRouteToAxpby) {
    e.eval("x = linspace(-1, 2, 6000); y = linspace(2, -1, 6000); a = 2; c = 3;");
    EXPECT_TRUE(sameOnOff(e, "a .* x + c .* y"));
}

// ---- axpby: a.*x ± b.*y (the fusedAxpby kernel) ------------------------

// Weighted sum / blend, and its coefficient commutations.
TEST_P(FusionParityTest, AxpbyAdd) {
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); "
           "y = reshape(linspace(5,-2,6000),2000,3); a = 0.25; b = 0.75;");
    EXPECT_TRUE(sameOnOff(e, "a .* x + b .* y"));
    EXPECT_TRUE(sameOnOff(e, "x .* a + y .* b"));
    e.eval("z = a .* x + b .* y;");
    EXPECT_NEAR(e.eval("z(1)").toScalar(), 0.25 * (-3.0) + 0.75 * 5.0, 1e-12);
}

// Scaled difference (b < 0 path via the `-` rule).
TEST_P(FusionParityTest, AxpbySub) {
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); "
           "y = reshape(linspace(5,-2,6000),2000,3); a = 2; b = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "a .* x - b .* y"));
}

// NaN / Inf flow through both arrays exactly as per-op.
TEST_P(FusionParityTest, AxpbyNaNInf) {
    e.eval("x = [linspace(-1,2,5997)'; NaN; Inf; -Inf]; "
           "y = [linspace(2,-1,5997)'; 1; 1; 1]; a = 3; b = -2;");
    EXPECT_TRUE(sameOnOff(e, "a .* x + b .* y"));
}

// Mismatched shapes (row + col → implicit expansion) must decline on the dims
// guard → per-op fallback; result still identical fused vs unfused.
TEST_P(FusionParityTest, AxpbyShapeMismatchFallsBack) {
    e.eval("x = linspace(-1, 2, 1100); y = linspace(2, -1, 1100)'; a = 2; b = 3;");
    EXPECT_TRUE(sameOnOff(e, "a .* x + b .* y"));
}

// ---- shift-scale: (x-c).*s and (x-c)./d (center then scale/divide) ------

TEST_P(FusionParityTest, ShiftScaleMul) {
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); c = 0.5; s = 2.5;");
    EXPECT_TRUE(sameOnOff(e, "(x - c) .* s"));
    EXPECT_TRUE(sameOnOff(e, "s .* (x - c)"));   // scale on the left
    e.eval("y = (x - c) .* s;");
    EXPECT_NEAR(e.eval("y(1)").toScalar(), (-3.0 - 0.5) * 2.5, 1e-12);
}

TEST_P(FusionParityTest, ShiftScaleDiv) {        // z-score / normalize
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); mu = 0.5; sg = 1.7;");
    EXPECT_TRUE(sameOnOff(e, "(x - mu) ./ sg"));
    e.eval("y = (x - mu) ./ sg;");
    EXPECT_NEAR(e.eval("y(1)").toScalar(), (-3.0 - 0.5) / 1.7, 1e-12);
}

TEST_P(FusionParityTest, ShiftScaleNaNInf) {
    e.eval("x = [linspace(-1,2,5997)'; NaN; Inf; -Inf]; c = 0.5; s = 2;");
    EXPECT_TRUE(sameOnOff(e, "(x - c) .* s"));
    EXPECT_TRUE(sameOnOff(e, "(x - c) ./ s"));
}

// `c - x` (scalar minus array) is not the (x-c) shape — the array lands in the
// shift slot, so the guard declines on size → per-op fallback, still identical.
TEST_P(FusionParityTest, ShiftScaleReversedFallsBack) {
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); c = 0.5; s = 2.5;");
    EXPECT_TRUE(sameOnOff(e, "(c - x) .* s"));
}

// ---- affine-clamp: max(lo, min(hi, a.*x ± b)) (normalize then saturate) -

TEST_P(FusionParityTest, AffineClampAdd) {
    e.eval("x = reshape(linspace(-5,5,6000),2000,3); a = 0.2; b = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "max(0, min(1, a .* x + b))"));
    EXPECT_TRUE(sameOnOff(e, "max(0, min(1, b + a .* x))"));  // commuted product
    e.eval("y = max(0, min(1, a .* x + b));");
    EXPECT_GE(e.eval("min(y(:))").toScalar(), 0.0);
    EXPECT_LE(e.eval("max(y(:))").toScalar(), 1.0);
}

TEST_P(FusionParityTest, AffineClampSub) {
    e.eval("x = reshape(linspace(-5,5,6000),2000,3); "
           "a = 0.2; b = 0.5; lo = -0.3; hi = 0.7;");
    EXPECT_TRUE(sameOnOff(e, "max(lo, min(hi, a .* x - b))"));
}

// NaN clamps exactly as per-op (min/max omit NaN → saturates to a bound).
TEST_P(FusionParityTest, AffineClampNaNInf) {
    e.eval("x = [linspace(-2,2,5997)'; NaN; Inf; -Inf]; a = 0.5; b = 0.1;");
    EXPECT_TRUE(sameOnOff(e, "max(0, min(1, a .* x + b))"));
}

// ---- min-outer clamp: min(hi, max(lo, x)) / min(hi, max(lo, a.*x±b)) ----

TEST_P(FusionParityTest, ClampMinOuter) {
    e.eval("x = reshape(linspace(-2, 3, 6000), 2000, 3); lo = 0.2; hi = 0.8;");
    EXPECT_TRUE(sameOnOff(e, "min(hi, max(lo, x))"));
    e.eval("y = min(hi, max(lo, x));");
    EXPECT_GE(e.eval("min(y(:))").toScalar(), 0.2);
    EXPECT_LE(e.eval("max(y(:))").toScalar(), 0.8);
}

TEST_P(FusionParityTest, AffineClampMinOuter) {
    e.eval("x = reshape(linspace(-5,5,6000),2000,3); a = 0.2; b = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "min(1, max(0, a .* x + b))"));
    EXPECT_TRUE(sameOnOff(e, "min(1, max(0, a .* x - b))"));
}

// NaN: min-outer saturates a NaN to lo (max(lo,NaN)=lo, min(hi,lo)=lo), the
// OPPOSITE of max-outer (→ hi). Both fused and unfused must agree per spelling.
TEST_P(FusionParityTest, ClampMinOuterNaNGoesToLo) {
    e.eval("x = [linspace(-2,2,5997)'; NaN; Inf; -Inf]; lo = 0.2; hi = 0.8;");
    EXPECT_TRUE(sameOnOff(e, "min(hi, max(lo, x))"));
    e.eval("ymin = min(hi, max(lo, x)); ymax = max(lo, min(hi, x));");
    EXPECT_EQ(e.eval("ymin(5998)").toScalar(), 0.2);   // min-outer NaN → lo
    EXPECT_EQ(e.eval("ymax(5998)").toScalar(), 0.8);   // max-outer NaN → hi
}

// ---- abs: abs(a.*x±b), abs(x-y), abs(x-c) -------------------------------

TEST_P(FusionParityTest, AbsAffine) {
    e.eval("x = reshape(linspace(-4,4,6000),2000,3); a = 1.5; b = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "abs(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "abs(a .* x - b)"));
    e.eval("y = abs(a .* x - b);");
    EXPECT_NEAR(e.eval("y(1)").toScalar(), std::abs(1.5 * (-4.0) - 0.5), 1e-12);
}

TEST_P(FusionParityTest, AbsDiffTwoArrays) {     // L1 residual
    e.eval("x = reshape(linspace(-4,4,6000),2000,3); "
           "y = reshape(linspace(3,-5,6000),2000,3);");
    EXPECT_TRUE(sameOnOff(e, "abs(x - y)"));
}

TEST_P(FusionParityTest, AbsDiffArrayScalar) {   // |x-c| and |c-x|
    e.eval("x = reshape(linspace(-4,4,6000),2000,3); c = 1.25;");
    EXPECT_TRUE(sameOnOff(e, "abs(x - c)"));
    EXPECT_TRUE(sameOnOff(e, "abs(c - x)"));
}

TEST_P(FusionParityTest, AbsNaNInf) {
    e.eval("x = [linspace(-2,2,5997)'; NaN; Inf; -Inf]; "
           "y = [linspace(2,-2,5997)'; 1; 1; 1]; a = 2; b = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "abs(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "abs(x - y)"));
}

// Broadcast (row - col) declines on the dims guard → per-op fallback.
TEST_P(FusionParityTest, AbsDiffShapeMismatchFallsBack) {
    e.eval("x = linspace(-2, 2, 1100); y = linspace(2, -2, 1100)';");
    EXPECT_TRUE(sameOnOff(e, "abs(x - y)"));
}

// ---- unary-affine: f(a.*x ± b), f ∈ {sqrt, floor, ceil} ----------------

TEST_P(FusionParityTest, SqrtAffineNonNegative) {
    e.eval("x = reshape(linspace(1,5,6000),2000,3); a = 2; b = 3;");  // affine in [5,13]
    EXPECT_TRUE(sameOnOff(e, "sqrt(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "sqrt(x .* a + b)"));
    e.eval("y = sqrt(a .* x + b);");
    EXPECT_NEAR(e.eval("y(1)").toScalar(), std::sqrt(2.0 * 1.0 + 3.0), 1e-12);
}

// Negative affine → MATLAB promotes to complex; fusion must decline so the
// per-op (complex) path runs. fused == unfused (both complex) either way.
TEST_P(FusionParityTest, SqrtAffineNegativePromotesComplex) {
    e.eval("x = reshape(linspace(-5,5,6000),2000,3); a = 1; b = 0;");
    EXPECT_TRUE(sameOnOff(e, "sqrt(a .* x + b)"));
    e.eval("y = sqrt(a .* x + b);");
    EXPECT_TRUE(e.eval("~isreal(y)").toBool());  // complex result preserved
}

TEST_P(FusionParityTest, FloorCeilAffine) {
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); a = 1.5; b = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "floor(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "ceil(a .* x - b)"));
    e.eval("yf = floor(a .* x + b); yc = ceil(a .* x + b);");
    EXPECT_NEAR(e.eval("yf(1)").toScalar(), std::floor(1.5 * (-3.0) + 0.5), 1e-12);
    EXPECT_NEAR(e.eval("yc(1)").toScalar(), std::ceil(1.5 * (-3.0) + 0.5), 1e-12);
}

// NaN/Inf flow through the real path (all affine values here are ≥ 0, so no
// complex promotion): sqrt(NaN)=NaN, sqrt(Inf)=Inf, floor/ceil likewise.
TEST_P(FusionParityTest, UnaryAffineNaNInf) {
    e.eval("x = [linspace(1,5,5997)'; NaN; Inf; 2]; a = 2; b = 1;");
    EXPECT_TRUE(sameOnOff(e, "sqrt(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "floor(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "ceil(a .* x + b)"));
}

// ---- square / magnitude: (a.*x±b).^2, (x-y).^2, (x-c).^2, sqrt(x.^2+y.^2) --

TEST_P(FusionParityTest, SqAffine) {
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); a = 1.5; b = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "(a .* x + b) .^ 2"));
    EXPECT_TRUE(sameOnOff(e, "(a .* x - b) .^ 2"));
    e.eval("y = (a .* x + b) .^ 2;");
    EXPECT_NEAR(e.eval("y(1)").toScalar(), 16.0, 1e-12);  // (1.5*-3+0.5)^2 = (-4)^2
}

TEST_P(FusionParityTest, SqDiffTwoArrays) {     // SSE term
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); "
           "y = reshape(linspace(2,-5,6000),2000,3);");
    EXPECT_TRUE(sameOnOff(e, "(x - y) .^ 2"));
}

TEST_P(FusionParityTest, SqDiffArrayScalar) {   // squared deviation (x-mu).^2
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); mu = 0.7;");
    EXPECT_TRUE(sameOnOff(e, "(x - mu) .^ 2"));
    EXPECT_TRUE(sameOnOff(e, "(mu - x) .^ 2"));   // == (x-mu).^2
}

TEST_P(FusionParityTest, SqrtSumSq) {            // magnitude / gradient mag
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); "
           "y = reshape(linspace(5,-2,6000),2000,3);");
    EXPECT_TRUE(sameOnOff(e, "sqrt(x .^ 2 + y .^ 2)"));
    e.eval("z = sqrt(x .^ 2 + y .^ 2);");
    EXPECT_GE(e.eval("min(z(:))").toScalar(), 0.0);
}

// NaN/Inf: squares stay real (sum of squares ≥ 0 → no complex promotion for
// the sqrt-sum form), and propagate as per-op.
TEST_P(FusionParityTest, SquareNaNInf) {
    e.eval("x = [linspace(-2,2,5997)'; NaN; Inf; -Inf]; "
           "y = [linspace(2,-2,5997)'; 1; 1; 1]; a = 2; b = 1;");
    EXPECT_TRUE(sameOnOff(e, "(a .* x + b) .^ 2"));
    EXPECT_TRUE(sameOnOff(e, "(x - y) .^ 2"));
    EXPECT_TRUE(sameOnOff(e, "sqrt(x .^ 2 + y .^ 2)"));
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

// axpby `a.*x + b.*y`: fusion collapses two products + a temporary into one
// streaming pass over three arrays. Confirms the two-array kernel fires.
namespace {
void probeAxpby(numkit::Engine::Backend backend, const char *tag) {
    numkit::StandardEngine e;
    e.setBackend(backend);
    e.eval("x = rand(3048*3816, 1); y = rand(3048*3816, 1); a = 0.3; b = 0.7;");
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval("z = a .* x + b .* y;");  // warm
        const int iters = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval("z = a .* x + b .* y;");
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] %s axpby 11.6M: fusion-off %.2f ms, fusion-on %.2f ms "
                "(%.2fx)\n", tag, off, on, off / on);
    EXPECT_LT(on, off * 0.90);  // fewer passes → faster → it fired
}
} // namespace

TEST(FusionFiringProbe, DISABLED_TreeWalkerAxpbySpeedup) {
    probeAxpby(numkit::Engine::Backend::TreeWalker, "TW");
}
TEST(FusionFiringProbe, DISABLED_VMAxpbySpeedup) {
    probeAxpby(numkit::Engine::Backend::VM, "VM");
}

// shift-scale `(x - c).*s`: fusion collapses subtract + temporary + scale into
// one streaming pass. Confirms the center-then-scale kernel fires.
namespace {
void probeShiftScale(numkit::Engine::Backend backend, const char *tag) {
    numkit::StandardEngine e;
    e.setBackend(backend);
    e.eval("x = rand(3048*3816, 1); c = 0.3; s = 2.0;");
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval("y = (x - c) .* s;");  // warm
        const int iters = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval("y = (x - c) .* s;");
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] %s shift-scale 11.6M: fusion-off %.2f ms, fusion-on "
                "%.2f ms (%.2fx)\n", tag, off, on, off / on);
    EXPECT_LT(on, off * 0.85);  // must be meaningfully faster → it fired
}
} // namespace

TEST(FusionFiringProbe, DISABLED_TreeWalkerShiftScaleSpeedup) {
    probeShiftScale(numkit::Engine::Backend::TreeWalker, "TW");
}
TEST(FusionFiringProbe, DISABLED_VMShiftScaleSpeedup) {
    probeShiftScale(numkit::Engine::Backend::VM, "VM");
}

// affine-clamp `max(0,min(1, a.*x+b))`: fusion collapses the whole 4-op
// normalize-then-saturate chain into one streaming pass. Confirms it fires.
namespace {
void probeAffineClamp(numkit::Engine::Backend backend, const char *tag) {
    numkit::StandardEngine e;
    e.setBackend(backend);
    e.eval("x = rand(3048*3816, 1); a = 0.5; b = 0.25;");
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval("y = max(0, min(1, a .* x + b));");  // warm
        const int iters = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval("y = max(0, min(1, a .* x + b));");
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] %s affine-clamp 11.6M: fusion-off %.2f ms, fusion-on "
                "%.2f ms (%.2fx)\n", tag, off, on, off / on);
    EXPECT_LT(on, off * 0.85);  // must be meaningfully faster → it fired
}
} // namespace

TEST(FusionFiringProbe, DISABLED_TreeWalkerAffineClampSpeedup) {
    probeAffineClamp(numkit::Engine::Backend::TreeWalker, "TW");
}
TEST(FusionFiringProbe, DISABLED_VMAffineClampSpeedup) {
    probeAffineClamp(numkit::Engine::Backend::VM, "VM");
}

// min-outer clamp `min(hi, max(lo, x))` — same kernel as max-outer, so this
// only confirms the min-outer matcher fires (parity passes even if it doesn't).
TEST(FusionFiringProbe, DISABLED_VMClampMinOuterSpeedup) {
    numkit::StandardEngine e;
    e.setBackend(numkit::Engine::Backend::VM);
    e.eval("x = rand(3048*3816, 1);");
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval("y = min(1, max(0, x));");  // warm
        const int iters = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval("y = min(1, max(0, x));");
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] VM clamp-min-outer 11.6M: fusion-off %.2f ms, "
                "fusion-on %.2f ms (%.2fx)\n", off, on, off / on);
    EXPECT_LT(on, off * 0.85);  // fired
}

// sq-affine `(a.*x+b).^2` and magnitude `sqrt(x.^2+y.^2)` firing probes.
namespace {
void probeFused(numkit::Engine::Backend backend, const char *tag,
                const char *setup, const char *expr) {
    numkit::StandardEngine e;
    e.setBackend(backend);
    e.eval(setup);
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval(std::string("z = ") + expr + ";");  // warm
        const int iters = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval(std::string("z = ") + expr + ";");
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] %s 11.6M: fusion-off %.2f ms, fusion-on %.2f ms "
                "(%.2fx)\n", tag, off, on, off / on);
    EXPECT_LT(on, off * 0.90);  // fired
}
} // namespace

TEST(FusionFiringProbe, DISABLED_VMSqAffineSpeedup) {
    probeFused(numkit::Engine::Backend::VM, "VM sq-affine",
               "x = rand(3048*3816,1); a = 1.5; b = 0.5;", "(a .* x + b) .^ 2");
}
TEST(FusionFiringProbe, DISABLED_VMSqrtSumSqSpeedup) {
    probeFused(numkit::Engine::Backend::VM, "VM sqrt-sumsq",
               "x = rand(3048*3816,1); y = rand(3048*3816,1);", "sqrt(x .^ 2 + y .^ 2)");
}

// abs-diff `abs(x - y)`: fusion collapses subtract + temporary + abs into one
// streaming pass over two arrays. Confirms the abs kernels fire.
namespace {
void probeAbsDiff(numkit::Engine::Backend backend, const char *tag) {
    numkit::StandardEngine e;
    e.setBackend(backend);
    e.eval("x = rand(3048*3816, 1); y = rand(3048*3816, 1);");
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval("z = abs(x - y);");  // warm
        const int iters = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval("z = abs(x - y);");
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] %s abs-diff 11.6M: fusion-off %.2f ms, fusion-on "
                "%.2f ms (%.2fx)\n", tag, off, on, off / on);
    EXPECT_LT(on, off * 0.90);  // fewer passes → faster → it fired
}
} // namespace

TEST(FusionFiringProbe, DISABLED_TreeWalkerAbsDiffSpeedup) {
    probeAbsDiff(numkit::Engine::Backend::TreeWalker, "TW");
}
TEST(FusionFiringProbe, DISABLED_VMAbsDiffSpeedup) {
    probeAbsDiff(numkit::Engine::Backend::VM, "VM");
}

// unary-affine `sqrt(a.*x+b)` (affine all-positive): fusion collapses the affine
// temporary + the sqrt pass into one. Confirms the unary-affine kernel fires.
namespace {
void probeSqrtAffine(numkit::Engine::Backend backend, const char *tag) {
    numkit::StandardEngine e;
    e.setBackend(backend);
    e.eval("x = rand(3048*3816, 1) * 4 + 1; a = 2; b = 0.5;");  // affine in [2.5, 10.5]
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval("y = sqrt(a .* x + b);");  // warm
        const int iters = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval("y = sqrt(a .* x + b);");
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] %s sqrt-affine 11.6M: fusion-off %.2f ms, fusion-on "
                "%.2f ms (%.2fx)\n", tag, off, on, off / on);
    EXPECT_LT(on, off * 0.90);  // fewer passes → faster → it fired
}
} // namespace

TEST(FusionFiringProbe, DISABLED_TreeWalkerSqrtAffineSpeedup) {
    probeSqrtAffine(numkit::Engine::Backend::TreeWalker, "TW");
}
TEST(FusionFiringProbe, DISABLED_VMSqrtAffineSpeedup) {
    probeSqrtAffine(numkit::Engine::Backend::VM, "VM");
}
