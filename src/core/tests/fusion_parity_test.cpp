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

// ---- axpby implicit a=1: a.*x ± y, x ± b.*y (the leaf side is an array) ---
// The trailing/leading "offset" leaf is a same-shape array, so affine_add/sub
// (and negprod for leaf - prod) route to fusedAxpby with the leaf coefficient 1.
TEST_P(FusionParityTest, AxpbyImplicitOne) {
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); "
           "y = reshape(linspace(5,-2,6000),2000,3); a = 0.4; b = 1.25;");
    EXPECT_TRUE(sameOnOff(e, "a .* x + y"));     // affine_add, array leaf
    EXPECT_TRUE(sameOnOff(e, "y + a .* x"));     // commuted
    EXPECT_TRUE(sameOnOff(e, "x + b .* y"));     // leaf + product
    EXPECT_TRUE(sameOnOff(e, "b .* y + x"));
    EXPECT_TRUE(sameOnOff(e, "a .* x - y"));     // affine_sub, array leaf
    EXPECT_TRUE(sameOnOff(e, "x - b .* y"));     // negprod: leaf - product
    e.eval("z = a .* x + y;");
    EXPECT_NEAR(e.eval("z(1)").toScalar(), 0.4 * (-3.0) + 5.0, 1e-12);
    e.eval("w = x - b .* y;");
    EXPECT_NEAR(e.eval("w(1)").toScalar(), -3.0 - 1.25 * 5.0, 1e-12);
}

// c - a.*x → the negated-scale affine fusedAffine(x, -a, c) (negprod, scalar leaf).
TEST_P(FusionParityTest, NegScaleAffine) {
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); a = 2.5; c = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "c - a .* x"));
    e.eval("y = c - a .* x;");
    EXPECT_NEAR(e.eval("y(1)").toScalar(), 0.5 - 2.5 * (-3.0), 1e-12);
}

// NaN / Inf flow through the implicit-1 axpby exactly as per-op.
TEST_P(FusionParityTest, AxpbyImplicitOneNaNInf) {
    e.eval("x = [linspace(-1,2,5997)'; NaN; Inf; -Inf]; "
           "y = [linspace(2,-1,5997)'; 1; 1; 1]; b = 2;");
    EXPECT_TRUE(sameOnOff(e, "x + b .* y"));
    EXPECT_TRUE(sameOnOff(e, "x - b .* y"));
    EXPECT_TRUE(sameOnOff(e, "b .* y - x"));     // axpby_sub (prod - leaf-array)
}

// Shape mismatch (row vs col) declines on the dims guard → per-op fallback.
TEST_P(FusionParityTest, AxpbyImplicitOneShapeMismatchFallsBack) {
    e.eval("x = linspace(-1,2,1100); y = linspace(2,-1,1100)'; b = 2;");
    EXPECT_TRUE(sameOnOff(e, "x + b .* y"));
    EXPECT_TRUE(sameOnOff(e, "x - b .* y"));
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

// fix (trunc toward zero) / round (half-away-from-zero) — same unary-affine
// family as floor/ceil. round mirrors numkit's CopySign+Trunc, NOT round-even.
TEST_P(FusionParityTest, FixRoundAffine) {
    e.eval("x = reshape(linspace(-4.5, 4.5, 6000), 2000, 3); a = 1.5; b = 0.25;");
    EXPECT_TRUE(sameOnOff(e, "fix(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "round(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "round(x - b)"));         // ShiftSub
    EXPECT_TRUE(sameOnOff(e, "fix(x ./ 2)"));          // div-inner
    EXPECT_TRUE(sameOnOff(e, "round(x ./ 2)"));
    // ties go half-away: round(0.5)=1, round(2.5)=3 (not round-even's 0/2).
    e.eval("t = [-2.5; -1.5; -0.5; 0.5; 1.5; 2.5]; tt = repmat(t, 1024, 1);");
    EXPECT_TRUE(sameOnOff(e, "round(1 .* tt + 0)"));
    e.eval("yr = round(1 .* tt + 0);");
    EXPECT_EQ(e.eval("yr(4)").toScalar(), 1.0);        // round(0.5) = 1
    EXPECT_EQ(e.eval("yr(6)").toScalar(), 3.0);        // round(2.5) = 3
    EXPECT_EQ(e.eval("yr(1)").toScalar(), -3.0);       // round(-2.5) = -3
}

TEST_P(FusionParityTest, FixRoundNaNInf) {
    e.eval("x = [linspace(-3,3,5998)'; NaN; Inf; -Inf]; a = 2; b = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "fix(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "round(a .* x + b)"));
}

// Broadened inner spellings: bare shift `f(x±c)` and bare product `f(a.*x)`.
TEST_P(FusionParityTest, UnaryAffineBroadInner) {
    e.eval("x = reshape(linspace(1,5,6000),2000,3); c = 0.5; a = 2;");
    EXPECT_TRUE(sameOnOff(e, "sqrt(x + c)"));     // ShiftAdd
    EXPECT_TRUE(sameOnOff(e, "sqrt(x - c)"));     // ShiftSub (x≥1>c → stays ≥0)
    EXPECT_TRUE(sameOnOff(e, "sqrt(a .* x)"));    // Product (no offset)
    EXPECT_TRUE(sameOnOff(e, "floor(x + c)"));
    EXPECT_TRUE(sameOnOff(e, "ceil(c - x)"));     // ShiftSub c-x → scale -1
    EXPECT_TRUE(sameOnOff(e, "floor(x .* a)"));
    e.eval("y = sqrt(x + c);");
    EXPECT_NEAR(e.eval("y(1)").toScalar(), std::sqrt(1.0 + 0.5), 1e-12);
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

// sq/abs/clamp migrated to matchInner: now fuse bare-product / shift / neg
// inner (not just product±leaf).
TEST_P(FusionParityTest, SqAbsClampBroadInner) {
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); a = 1.5; c = 0.5; s = 2;");
    EXPECT_TRUE(sameOnOff(e, "(a .* x) .^ 2"));          // bare product
    EXPECT_TRUE(sameOnOff(e, "(x + c) .^ 2"));           // shift-add
    EXPECT_TRUE(sameOnOff(e, "(-x) .^ 2"));              // neg
    EXPECT_TRUE(sameOnOff(e, "abs(a .* x)"));
    EXPECT_TRUE(sameOnOff(e, "abs(x + c)"));
    EXPECT_TRUE(sameOnOff(e, "abs(-x)"));
    EXPECT_TRUE(sameOnOff(e, "max(0, min(1, x .* s))"));
    EXPECT_TRUE(sameOnOff(e, "max(0, min(1, x + c))"));
    EXPECT_TRUE(sameOnOff(e, "min(1, max(0, x .* s))")); // min-outer, bare product
}

// Regression guard: the 2-array diffs must STILL fuse after the migration
// (sq/abs skip ShiftSub so they don't shadow the _diff rules).
TEST_P(FusionParityTest, SqAbsDiffStillFuse) {
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); "
           "y = reshape(linspace(2,-5,6000),2000,3);");
    EXPECT_TRUE(sameOnOff(e, "(x - y) .^ 2"));            // sq_diff (2-array)
    EXPECT_TRUE(sameOnOff(e, "abs(x - y)"));               // abs_diff (2-array)
    EXPECT_TRUE(sameOnOff(e, "(x - 0.5) .^ 2"));           // sq_diff (array-scalar)
    EXPECT_TRUE(sameOnOff(e, "abs(x - 0.5)"));             // abs_diff (array-scalar)
}

// ---- soft-threshold: sign(x) .* max(0, abs(x) - t) ---------------------

TEST_P(FusionParityTest, SoftThreshold) {
    e.eval("x = reshape(linspace(-5,5,6000),2000,3); t = 1.5;");
    EXPECT_TRUE(sameOnOff(e, "sign(x) .* max(0, abs(x) - t)"));
    EXPECT_TRUE(sameOnOff(e, "sign(x) .* max(abs(x) - t, 0)"));  // 0 as 2nd arg
    e.eval("y = sign(x) .* max(0, abs(x) - t);");
    EXPECT_NEAR(e.eval("y(1)").toScalar(), -3.5, 1e-12);  // x(1)=-5 → -(5-1.5)
}

TEST_P(FusionParityTest, SoftThresholdNaNInf) {
    e.eval("x = [linspace(-3,3,5997)'; NaN; Inf; -Inf]; t = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "sign(x) .* max(0, abs(x) - t)"));
}

// Different variables in sign()/abs() → not the shape; declines at match,
// per-op result still identical.
TEST_P(FusionParityTest, SoftThresholdDifferentVarsFallBack) {
    e.eval("x = reshape(linspace(-5,5,6000),2000,3); "
           "w = reshape(linspace(5,-5,6000),2000,3); t = 1.5;");
    EXPECT_TRUE(sameOnOff(e, "sign(x) .* max(0, abs(w) - t)"));
}

// ---- transcendental-affine: exp/expm1(a.*x ± b) ------------------------
// n = 6001 (not a multiple of the SIMD width) so the per-chunk scalar tail is
// non-empty — this is exactly what would diverge if the kernel didn't mirror
// numkit's exp loop (hn:: body + std:: tail, same chunking).

TEST_P(FusionParityTest, ExpAffine) {
    e.eval("x = linspace(-2, 2, 6001)'; a = 1.5; b = 0.3;");
    EXPECT_TRUE(sameOnOff(e, "exp(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "exp(b + a .* x)"));
    EXPECT_TRUE(sameOnOff(e, "exp(a .* x - b)"));
}

TEST_P(FusionParityTest, Expm1Affine) {
    e.eval("x = linspace(-2, 2, 6001)'; a = 2; b = 0.1;");
    EXPECT_TRUE(sameOnOff(e, "expm1(a .* x + b)"));
}

// exp of any real is real: overflow→Inf, underflow→0, NaN→NaN; all match per-op.
TEST_P(FusionParityTest, ExpAffineEdges) {
    e.eval("x = [linspace(-2,2,5998)'; 1000; -1000; NaN]; a = 1; b = 0;");
    EXPECT_TRUE(sameOnOff(e, "exp(a .* x + b)"));
    e.eval("y = exp(a .* x + b);");
    EXPECT_TRUE(e.eval("isinf(y(5999))").toBool());   // exp(1000) = Inf
    EXPECT_EQ(e.eval("y(6000)").toScalar(), 0.0);      // exp(-1000) = 0
}

// Broadened inner spellings for exp/expm1 (incl. the n=6001 SIMD-tail check).
TEST_P(FusionParityTest, ExpAffineBroadInner) {
    e.eval("x = linspace(-2, 2, 6001)'; c = 0.5; a = 2;");
    EXPECT_TRUE(sameOnOff(e, "exp(x + c)"));     // ShiftAdd
    EXPECT_TRUE(sameOnOff(e, "exp(x - c)"));     // ShiftSub
    EXPECT_TRUE(sameOnOff(e, "exp(c - x)"));     // ShiftSub c-x → scale -1
    EXPECT_TRUE(sameOnOff(e, "exp(a .* x)"));    // Product
    EXPECT_TRUE(sameOnOff(e, "expm1(x - c)"));
}

// Unary minus: exp(-k*x) parses as exp((-k)*x) (negated coefficient, via the
// isPureLeaf `-leaf` extension), exp(-x) is the NegLeaf inner, and `+(-b)` is a
// negated offset. All bit-exact (negation is exact).
TEST_P(FusionParityTest, UnaryMinusInner) {
    e.eval("x = reshape(linspace(-3,3,6000),2000,3); k = 2.5; b = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "exp(-k * x)"));         // (-k)*x — negated coeff
    EXPECT_TRUE(sameOnOff(e, "exp(-x)"));             // NegLeaf
    EXPECT_TRUE(sameOnOff(e, "tanh(-k .* x)"));
    EXPECT_TRUE(sameOnOff(e, "exp(k .* x + (-b))"));  // negated offset
    EXPECT_TRUE(sameOnOff(e, "k .* x + (-b)"));       // plain affine, negated offset
    EXPECT_TRUE(sameOnOff(e, "floor(-x)"));
}

// log / log2 / log10 of a (positive) affine. n=6001 forces the SIMD tail.
TEST_P(FusionParityTest, LogAffine) {
    e.eval("x = linspace(0.1, 5, 6001)'; a = 1.5; b = 0.3;");  // affine > 0
    EXPECT_TRUE(sameOnOff(e, "log(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "log2(x + 1)"));       // ShiftAdd inner
    EXPECT_TRUE(sameOnOff(e, "log10(a .* x)"));      // Product inner (dB-ish)
}

// log of a negative affine → MATLAB complex; the rule declines, per-op runs.
TEST_P(FusionParityTest, LogAffineNegativePromotesComplex) {
    e.eval("x = linspace(-2, 2, 6001)'; a = 1; b = 0;");
    EXPECT_TRUE(sameOnOff(e, "log(a .* x + b)"));
    e.eval("y = log(a .* x + b);");
    EXPECT_TRUE(e.eval("~isreal(y)").toBool());      // complex preserved
}

// sin / cos / tanh (always-real) of an affine, across inner spellings.
TEST_P(FusionParityTest, TrigAffine) {
    e.eval("x = linspace(-3, 3, 6001)'; a = 2; b = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "sin(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "cos(x - b)"));         // ShiftSub inner
    EXPECT_TRUE(sameOnOff(e, "tanh(a .* x)"));       // Product inner
}

TEST_P(FusionParityTest, TrigAffineNaNInf) {
    e.eval("x = [linspace(-3,3,5999)'; NaN; Inf]; a = 1; b = 0;");
    EXPECT_TRUE(sameOnOff(e, "sin(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "tanh(a .* x + b)"));    // tanh(Inf)=1, tanh(NaN)=NaN
}

// sinh / atan / asinh (always-real, single-hn:: mirror).
TEST_P(FusionParityTest, MoreTransAffine) {
    e.eval("x = linspace(-3, 3, 6001)'; a = 1.5; b = 0.2;");
    EXPECT_TRUE(sameOnOff(e, "sinh(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "atan(a .* x)"));        // Product inner
    EXPECT_TRUE(sameOnOff(e, "asinh(x - b)"));        // ShiftSub inner
}

// Rare transcendentals — asin/acos/atanh (|inner|>1 → complex), acosh (inner<1),
// log1p (inner<-1), cosh (always-real, composed 0.5(e^v+e^-v)). In-domain inputs
// fuse via fusible inners (bare args don't match an InnerKind); n=6001 = SIMD tail.
TEST_P(FusionParityTest, RareTransAffineInDomain) {
    e.eval("x = linspace(-0.8, 0.8, 6001)'; a = 0.5; b = 0.1;");  // affine ⊂ (-1,1)
    EXPECT_TRUE(sameOnOff(e, "asin(a .* x + b)"));    // ProductAdd
    EXPECT_TRUE(sameOnOff(e, "acos(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "atanh(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "asin(x - b)"));         // ShiftSub
    EXPECT_TRUE(sameOnOff(e, "atanh(a .* x)"));       // Product
    e.eval("xc = linspace(1.1, 5, 6001)';");
    EXPECT_TRUE(sameOnOff(e, "acosh(0.5 .* xc + 1)"));   // affine ≥1, ProductAdd
    EXPECT_TRUE(sameOnOff(e, "acosh(xc + 0.5)"));        // ShiftAdd ≥1
    e.eval("xp = linspace(-0.5, 5, 6001)';");
    EXPECT_TRUE(sameOnOff(e, "log1p(xp + 0.5)"));        // ShiftAdd > -1
    EXPECT_TRUE(sameOnOff(e, "log1p(2 .* xp)"));         // Product (2*-0.5=-1 → -Inf, real)
    e.eval("xa = linspace(-3, 3, 6001)';");
    EXPECT_TRUE(sameOnOff(e, "cosh(a .* xa + b)"));      // always-real
    EXPECT_TRUE(sameOnOff(e, "cosh(-xa)"));              // NegLeaf
    e.eval("y = cosh(a .* xa + b);");
    EXPECT_GE(e.eval("min(y(:))").toScalar(), 1.0);      // cosh ≥ 1
}

// Out-of-domain → MATLAB complex; the rule declines so the per-op (complex) path
// runs. fused == unfused (both complex) either way.
TEST_P(FusionParityTest, RareTransAffineDomainDeclines) {
    e.eval("x = linspace(-3, 3, 6001)'; a = 1; b = 0;");  // affine spans outside [-1,1]
    EXPECT_TRUE(sameOnOff(e, "asin(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "acos(a .* x + b)"));
    EXPECT_TRUE(sameOnOff(e, "atanh(a .* x + b)"));
    EXPECT_TRUE(e.eval("~isreal(asin(a .* x + b))").toBool());
    e.eval("xl = linspace(-5, 5, 6001)';");
    EXPECT_TRUE(sameOnOff(e, "acosh(1 .* xl + 0)"));      // <1 somewhere → complex
    EXPECT_TRUE(sameOnOff(e, "log1p(1 .* xl + 0)"));      // <-1 somewhere → complex
    EXPECT_TRUE(e.eval("~isreal(acosh(1 .* xl + 0))").toBool());
    EXPECT_TRUE(e.eval("~isreal(log1p(1 .* xl + 0))").toBool());
}

// NaN/Inf through always-real cosh; NaN stays real for the in-(-1,1) inverse fns
// (NaN fails the |·|>1 decline check, so asin(NaN)=NaN like per-op).
TEST_P(FusionParityTest, RareTransAffineNaNInf) {
    e.eval("xa = [linspace(-2,2,5999)'; NaN; Inf]; a = 1; b = 0;");
    EXPECT_TRUE(sameOnOff(e, "cosh(a .* xa + b)"));       // cosh(Inf)=Inf, cosh(NaN)=NaN
    e.eval("xu = [linspace(-0.8,0.8,5999)'; NaN; -0.5];");
    EXPECT_TRUE(sameOnOff(e, "asin(1 .* xu + 0)"));       // NaN stays real, no promotion
    EXPECT_TRUE(sameOnOff(e, "atanh(1 .* xu + 0)"));
}

// div-inner for the rare transcendentals (the shift-div kernel + domain decline).
TEST_P(FusionParityTest, RareTransDivInner) {
    e.eval("x = linspace(-2, 2, 6001)'; d = 4; c = 0.5;");   // x/d, (x-c)/d ⊂ (-1,1)
    EXPECT_TRUE(sameOnOff(e, "asin(x ./ d)"));
    EXPECT_TRUE(sameOnOff(e, "cosh(x ./ d)"));
    EXPECT_TRUE(sameOnOff(e, "atanh((x - c) ./ d)"));
    e.eval("xc = linspace(5, 9, 6001)';");
    EXPECT_TRUE(sameOnOff(e, "acosh(xc ./ d)"));          // xc/4 ⊂ [1.25,2.25] ≥1
    e.eval("xbad = linspace(-9, 9, 6001)';");
    EXPECT_TRUE(sameOnOff(e, "asin(xbad ./ d)"));         // some |x/4|>1 → complex
    EXPECT_TRUE(e.eval("~isreal(asin(xbad ./ d))").toBool());
}

// tan — no Highway primitive; the fused kernel mirrors numkit's TanLoop (TanVec
// on |inner|<1e6, per-block std::tan otherwise). Always-real. n=6001 SIMD tail.
TEST_P(FusionParityTest, TanAffine) {
    e.eval("x = linspace(-3, 3, 6001)'; a = 1.5; b = 0.2;");
    EXPECT_TRUE(sameOnOff(e, "tan(a .* x + b)"));     // ProductAdd
    EXPECT_TRUE(sameOnOff(e, "tan(x - b)"));          // ShiftSub
    EXPECT_TRUE(sameOnOff(e, "tan(a .* x)"));         // Product
    EXPECT_TRUE(sameOnOff(e, "tan(x ./ 2)"));         // div-inner
    EXPECT_TRUE(sameOnOff(e, "tan(-x)"));             // NegLeaf
}

// Force the per-block |inner|>=1e6 scalar fallback (numkit's TanLoop drops to
// std::tan for the whole block there) AND a non-multiple length so the scalar
// tail runs too. Large args + NaN/Inf are interleaved so blocks mix both paths.
TEST_P(FusionParityTest, TanAffineLargeArgFallback) {
    e.eval("x = [linspace(-3,3,5990)'; 2e6; -2e6; 1e7; 5e5; NaN; Inf; -Inf; "
           "3e6; 7e6; 1.5e6; 4e5];");                 // 5990 + 11 = 6001
    EXPECT_TRUE(sameOnOff(e, "tan(1 .* x + 0)"));     // affine inner = x, spans 1e6
    EXPECT_TRUE(sameOnOff(e, "tan(x ./ 3)"));         // div-inner, large args
}

// div-inner: f(x./d) and f((x-c)./d) via the dedicated shift-div kernels.
TEST_P(FusionParityTest, DivInner) {
    e.eval("x = reshape(linspace(1,9,6000),2000,3); d = 2.5; c = 0.5;");  // x>0
    EXPECT_TRUE(sameOnOff(e, "sqrt(x ./ d)"));            // x/d
    EXPECT_TRUE(sameOnOff(e, "sqrt((x - c) ./ d)"));      // (x-c)/d
    EXPECT_TRUE(sameOnOff(e, "floor(x ./ d)"));
    EXPECT_TRUE(sameOnOff(e, "exp(x ./ d)"));
    EXPECT_TRUE(sameOnOff(e, "log((x - c) ./ d)"));       // log domain ok (x≥1>c)
    EXPECT_TRUE(sameOnOff(e, "exp((c - x) ./ d)"));        // (c-x)/d → (x-c)/(-d)
    e.eval("y = sqrt(x ./ d);");
    EXPECT_NEAR(e.eval("y(1)").toScalar(), std::sqrt(1.0 / 2.5), 1e-12);
}

// div-inner trans on n=6001 (SIMD tail) + log negative→complex decline.
TEST_P(FusionParityTest, DivInnerTransTailAndDomain) {
    e.eval("x = linspace(0.1, 5, 6001)'; d = 3; c = 0.2;");
    EXPECT_TRUE(sameOnOff(e, "exp(x ./ d)"));
    EXPECT_TRUE(sameOnOff(e, "log(x ./ d)"));
    e.eval("xn = linspace(-4, 4, 6001)'; ");
    EXPECT_TRUE(sameOnOff(e, "log(xn ./ d)"));            // negatives → complex, declines
    EXPECT_TRUE(e.eval("~isreal(log(xn ./ d))").toBool());
}

// div-inner for sq / abs / clamp: (x./d).^2, abs((x-c)./d), rescale-then-
// saturate max(0,min(1,(x-c)./d)). matchDivArg routes these to the dedicated
// shift-div kernels (matchInner rejects `./`, so the affine families decline).
TEST_P(FusionParityTest, DivInnerSqAbsClamp) {
    e.eval("x = reshape(linspace(1,9,6000),2000,3); d = 2.5; c = 0.5; lo = 0; hi = 1;");
    EXPECT_TRUE(sameOnOff(e, "(x ./ d) .^ 2"));                // sq_div, x/d
    EXPECT_TRUE(sameOnOff(e, "((x - c) ./ d) .^ 2"));          // sq_div, (x-c)/d
    EXPECT_TRUE(sameOnOff(e, "abs(x ./ d)"));                  // abs_div
    EXPECT_TRUE(sameOnOff(e, "abs((c - x) ./ d)"));            // (c-x)/d → (x-c)/(-d)
    EXPECT_TRUE(sameOnOff(e, "max(0, min(1, (x - c) ./ d))")); // clamp_div max-outer
    EXPECT_TRUE(sameOnOff(e, "min(hi, max(lo, x ./ d))"));     // clamp_div min-outer
    e.eval("y = (x ./ d) .^ 2;");
    EXPECT_NEAR(e.eval("y(1)").toScalar(), (1.0 / 2.5) * (1.0 / 2.5), 1e-12);
    e.eval("z = max(0, min(1, (x - c) ./ d));");
    EXPECT_GE(e.eval("min(z(:))").toScalar(), 0.0);
    EXPECT_LE(e.eval("max(z(:))").toScalar(), 1.0);
}

// NaN / Inf through the divide-inner sq/abs/clamp kernels.
TEST_P(FusionParityTest, DivInnerSqAbsClampNaNInf) {
    e.eval("x = [linspace(-2,2,5997)'; NaN; Inf; -Inf]; d = 2; c = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "(x ./ d) .^ 2"));
    EXPECT_TRUE(sameOnOff(e, "abs((x - c) ./ d)"));
    EXPECT_TRUE(sameOnOff(e, "max(0, min(1, x ./ d))"));
    EXPECT_TRUE(sameOnOff(e, "min(1, max(0, (x - c) ./ d))"));
}

// ---- complex inputs: arithmetic (affine / axpby / shift-scale / sq) -------
// Fusion fires when the ARRAY is complex; scalar coefficients may be real or
// complex. The complex kernels are scalar std::complex loops that mirror
// numkit's per-op std::complex composition bit-for-bit (real coeff a → the FULL
// complex mul Complex(a,0)*z, exactly as numkit's promoteToComplex path).
TEST_P(FusionParityTest, ComplexAffine) {
    e.eval("z = reshape(linspace(-3,4,6000),2000,3) + "
           "1i*reshape(linspace(2,-5,6000),2000,3); "
           "a = 2.5; b = -0.75; ca = 1+2i; cb = 0.5-1i;");
    EXPECT_TRUE(sameOnOff(e, "a .* z + b"));        // real coeffs, complex array
    EXPECT_TRUE(sameOnOff(e, "z .* a + b"));
    EXPECT_TRUE(sameOnOff(e, "ca .* z + cb"));      // complex coeffs
    EXPECT_TRUE(sameOnOff(e, "a .* z - b"));
    EXPECT_TRUE(sameOnOff(e, "cb + ca .* z"));      // commuted
    EXPECT_TRUE(sameOnOff(e, "cb - ca .* z"));      // negprod: c - a.*z
    EXPECT_TRUE(e.eval("~isreal(ca .* z + cb)").toBool());
}

TEST_P(FusionParityTest, ComplexAxpby) {
    e.eval("z = reshape(linspace(-3,4,6000),2000,3) + 1i*reshape(linspace(1,-2,6000),2000,3); "
           "w = reshape(linspace(2,-5,6000),2000,3) + 1i*reshape(linspace(-1,3,6000),2000,3); "
           "a = 0.25; b = 0.75; ca = 1+1i; cb = 2-1i;");
    EXPECT_TRUE(sameOnOff(e, "a .* z + b .* w"));    // both products complex
    EXPECT_TRUE(sameOnOff(e, "ca .* z - cb .* w"));
    EXPECT_TRUE(sameOnOff(e, "z + b .* w"));         // implicit a=1
    EXPECT_TRUE(sameOnOff(e, "z - cb .* w"));         // negprod, array leaf
}

TEST_P(FusionParityTest, ComplexShiftScaleSq) {
    e.eval("z = reshape(linspace(-3,4,6000),2000,3) + 1i*reshape(linspace(2,-5,6000),2000,3); "
           "w = reshape(linspace(1,-1,6000),2000,3) + 1i*reshape(linspace(-2,2,6000),2000,3); "
           "c = 0.5; s = 2.5; cc = 1+1i; cs = 0.5-0.5i; a = 1.5; b = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "(z - c) .* s"));
    EXPECT_TRUE(sameOnOff(e, "(z - cc) ./ cs"));
    EXPECT_TRUE(sameOnOff(e, "(z - c) ./ s"));
    EXPECT_TRUE(sameOnOff(e, "(a .* z + b) .^ 2"));   // Product-kind complex sq
    EXPECT_TRUE(sameOnOff(e, "(cc .* z) .^ 2"));       // bare product
    EXPECT_TRUE(sameOnOff(e, "(z - w) .^ 2"));         // two-array complex sq-diff
}

// NaN/Inf: numkit promotes a real coeff to Complex(a,0) and does the FULL
// complex multiply, so inf*0=NaN appears — both fused and per-op the same way.
TEST_P(FusionParityTest, ComplexArithNaNInf) {
    e.eval("z = [linspace(-2,2,5997)'; NaN; Inf; -Inf] + "
           "1i*[linspace(1,-1,5997)'; 1; Inf; 2]; a = 1.5; b = 0.5; ca = 1+1i;");
    EXPECT_TRUE(sameOnOff(e, "a .* z + b"));
    EXPECT_TRUE(sameOnOff(e, "ca .* z + b"));
    EXPECT_TRUE(sameOnOff(e, "(a .* z + b) .^ 2"));
    EXPECT_TRUE(sameOnOff(e, "(z - ca) .* a"));
}

// Real-array + complex coefficient is NOT fused (declines → per-op promotes the
// array); still identical fused vs unfused.
TEST_P(FusionParityTest, ComplexCoeffRealArrayFallsBack) {
    e.eval("x = reshape(linspace(-3,4,6000),2000,3); ca = 1+2i;");
    EXPECT_TRUE(sameOnOff(e, "ca .* x + 1"));
    EXPECT_TRUE(e.eval("~isreal(ca .* x + 1)").toBool());
}

// ---- complex inputs: unary (sqrt) + transcendentals -----------------------
// sqrt(z) = std::sqrt(z); transcendentals mirror numkit's complex op per fn
// (std::F(z); log2 = log(z)/log(2); log1p = log(1+z)). Complex is total — no
// domain declines. Product-kind inner only (shift/neg kinds decline on complex).
TEST_P(FusionParityTest, ComplexUnaryTrans) {
    e.eval("z = reshape(linspace(-3,4,6000),2000,3) + "
           "1i*reshape(linspace(2,-5,6000),2000,3); "
           "a = 1.5; b = 0.5; ca = 1+1i; d = 2.5;");
    EXPECT_TRUE(sameOnOff(e, "sqrt(a .* z + b)"));   // Product-kind complex sqrt
    EXPECT_TRUE(sameOnOff(e, "sqrt(ca .* z)"));
    EXPECT_TRUE(sameOnOff(e, "sqrt(z ./ d)"));        // div-inner
    EXPECT_TRUE(sameOnOff(e, "exp(a .* z + b)"));
    EXPECT_TRUE(sameOnOff(e, "log(ca .* z + b)"));
    EXPECT_TRUE(sameOnOff(e, "log2(a .* z + b)"));    // log(z)/log(2)
    EXPECT_TRUE(sameOnOff(e, "log10(a .* z)"));
    EXPECT_TRUE(sameOnOff(e, "log1p(a .* z + b)"));   // log(1+z)
    EXPECT_TRUE(sameOnOff(e, "sin(a .* z)"));
    EXPECT_TRUE(sameOnOff(e, "cos(a .* z + b)"));
    EXPECT_TRUE(sameOnOff(e, "tan(a .* z)"));
    EXPECT_TRUE(sameOnOff(e, "tanh(a .* z + b)"));
    EXPECT_TRUE(sameOnOff(e, "sinh(a .* z)"));
    EXPECT_TRUE(sameOnOff(e, "cosh(a .* z + b)"));
    EXPECT_TRUE(sameOnOff(e, "atan(a .* z)"));
    EXPECT_TRUE(sameOnOff(e, "asinh(a .* z + b)"));
    EXPECT_TRUE(sameOnOff(e, "asin(a .* z)"));
    EXPECT_TRUE(sameOnOff(e, "acos(a .* z + b)"));
    EXPECT_TRUE(sameOnOff(e, "acosh(ca .* z)"));
    EXPECT_TRUE(sameOnOff(e, "atanh(a .* z + b)"));
    EXPECT_TRUE(sameOnOff(e, "exp(z ./ d)"));          // div-inner trans
    EXPECT_TRUE(sameOnOff(e, "log((z - ca) ./ d)"));
    EXPECT_TRUE(e.eval("~isreal(exp(a .* z + b))").toBool());
}

// Complex floor/ceil/fix/round (applied component-wise to re+im) and expm1
// (= exp(z)-1) now fuse — per-op gained complex support (commit b2f30c53), so
// the fused kernels mirror it bit-for-bit. Fractional re AND im so the rounding
// is observable on both parts.
TEST_P(FusionParityTest, ComplexRoundLikeExpm1) {
    e.eval("z = reshape(linspace(-3.4, 4.6, 6000), 2000, 3) + "
           "1i*reshape(linspace(2.5, -5.5, 6000), 2000, 3); "
           "a = 1.5; b = 0.5; ca = 1+1i; d = 2.5;");
    EXPECT_TRUE(sameOnOff(e, "floor(a .* z + b)"));
    EXPECT_TRUE(sameOnOff(e, "ceil(ca .* z)"));
    EXPECT_TRUE(sameOnOff(e, "fix(a .* z - b)"));
    EXPECT_TRUE(sameOnOff(e, "round(a .* z + b)"));      // half-away, component-wise
    EXPECT_TRUE(sameOnOff(e, "floor(z ./ d)"));          // div-inner
    EXPECT_TRUE(sameOnOff(e, "round((z - ca) ./ d)"));
    EXPECT_TRUE(sameOnOff(e, "expm1(a .* z + b)"));       // exp(z)-1
    EXPECT_TRUE(sameOnOff(e, "expm1(ca .* z)"));
    EXPECT_TRUE(sameOnOff(e, "expm1(z ./ d)"));           // div-inner
    // component-wise rounding keeps the imaginary part → result stays complex
    EXPECT_TRUE(e.eval("~isreal(floor(a .* z + b))").toBool());
    EXPECT_TRUE(e.eval("~isreal(expm1(a .* z + b))").toBool());
}

// Complex shift/neg inner kinds: f(z±c), f(c-z), f(-z), (z+c).^2, abs(-z) — the
// inner is a genuine add/negate (scale ±1), NOT a complex mul by (±1+0i). sqrt/
// trans take all inner kinds; sq/abs take ShiftAdd/NegLeaf (ShiftSub → the _diff
// rules).
TEST_P(FusionParityTest, ComplexShiftNegInner) {
    e.eval("z = reshape(linspace(-3,4,6000),2000,3) + "
           "1i*reshape(linspace(2,-5,6000),2000,3); c = 0.5; cc = 1+1i;");
    EXPECT_TRUE(sameOnOff(e, "exp(z + c)"));        // ShiftAdd (real c)
    EXPECT_TRUE(sameOnOff(e, "exp(z + cc)"));        // ShiftAdd (complex c)
    EXPECT_TRUE(sameOnOff(e, "exp(z - cc)"));        // ShiftSub z-c
    EXPECT_TRUE(sameOnOff(e, "exp(cc - z)"));        // ShiftSub c-z (scale -1)
    EXPECT_TRUE(sameOnOff(e, "exp(-z)"));             // NegLeaf
    EXPECT_TRUE(sameOnOff(e, "sqrt(z + cc)"));
    EXPECT_TRUE(sameOnOff(e, "cos(cc - z)"));
    EXPECT_TRUE(sameOnOff(e, "(z + cc) .^ 2"));       // sq ShiftAdd
    EXPECT_TRUE(sameOnOff(e, "(-z) .^ 2"));            // sq NegLeaf
    EXPECT_TRUE(sameOnOff(e, "abs(z + cc)"));          // abs ShiftAdd → real
    EXPECT_TRUE(sameOnOff(e, "abs(-z)"));              // abs NegLeaf → real
}

// The mul-by-1 hazard, exercised: shift/neg inner on a complex array with Inf
// components. Per-op does a bare add/negate (no NaN); a complex mul by (±1+0i)
// would make 0*Inf=NaN and diverge — this confirms the kernel does add/negate.
TEST_P(FusionParityTest, ComplexShiftNegInnerNonFinite) {
    e.eval("re = [linspace(-2,2,5998)'; Inf; -3]; im = [linspace(1,-1,5998)'; 2; Inf]; "
           "z = complex(re, im); cc = 1+1i;");
    EXPECT_TRUE(sameOnOff(e, "exp(z + cc)"));
    EXPECT_TRUE(sameOnOff(e, "exp(-z)"));
    EXPECT_TRUE(sameOnOff(e, "(z + cc) .^ 2"));
    EXPECT_TRUE(sameOnOff(e, "abs(z + cc)"));
    EXPECT_TRUE(sameOnOff(e, "cos(cc - z)"));
}

// NaN/Inf through the complex transcendentals (std::F(complex) handles them; the
// fused kernel calls the same std::F, so identical).
TEST_P(FusionParityTest, ComplexTransNaNInf) {
    e.eval("z = [linspace(-2,2,5998)'; NaN; Inf] + "
           "1i*[linspace(1,-1,5998)'; 1; 2]; a = 1; b = 0;");
    EXPECT_TRUE(sameOnOff(e, "exp(a .* z + b)"));
    EXPECT_TRUE(sameOnOff(e, "sin(a .* z + b)"));
    EXPECT_TRUE(sameOnOff(e, "sqrt(a .* z + b)"));
}

// ---- complex inputs: abs (→ real magnitude) + soft-threshold --------------
// abs(z) = std::abs(z) is REAL; complex soft-threshold sign(z).*max(0,|z|-t) is
// complex (sign(z)=z/|z|). clamp/min-max and sqrt-sumsq on complex are left to
// per-op (exotic / niche), and array-scalar complex abs-diff declines.
TEST_P(FusionParityTest, ComplexAbsSoft) {
    e.eval("z = reshape(linspace(-3,4,6000),2000,3) + "
           "1i*reshape(linspace(2,-5,6000),2000,3); "
           "w = reshape(linspace(1,-1,6000),2000,3) + "
           "1i*reshape(linspace(-2,2,6000),2000,3); "
           "a = 1.5; b = 0.5; ca = 1+1i; d = 2.5; t = 1.5;");
    EXPECT_TRUE(sameOnOff(e, "abs(a .* z + b)"));      // |a.*z+b| → real
    EXPECT_TRUE(sameOnOff(e, "abs(ca .* z)"));         // bare product
    EXPECT_TRUE(sameOnOff(e, "abs(z ./ d)"));          // div-inner abs
    EXPECT_TRUE(sameOnOff(e, "abs(z - w)"));            // two-array |z-w|
    EXPECT_TRUE(sameOnOff(e, "sign(z) .* max(0, abs(z) - t)"));  // complex soft-threshold
    EXPECT_TRUE(e.eval("isreal(abs(a .* z + b))").toBool());     // abs(complex) is real
    EXPECT_TRUE(e.eval("~isreal(sign(z) .* max(0, abs(z) - t))").toBool());
}

TEST_P(FusionParityTest, ComplexAbsSoftNaNInf) {
    e.eval("z = [linspace(-2,2,5998)'; NaN; Inf] + "
           "1i*[linspace(1,-1,5998)'; 1; 2]; a = 1.5; b = 0.5; t = 0.5;");
    EXPECT_TRUE(sameOnOff(e, "abs(a .* z + b)"));
    EXPECT_TRUE(sameOnOff(e, "abs(z - (1+1i))"));       // |z-c| array-scalar declines → per-op
    EXPECT_TRUE(sameOnOff(e, "sign(z) .* max(0, abs(z) - t)"));
}

// ---- complex: array-scalar diff, sqrt-sumsq, sq-div, reversed div ----------
TEST_P(FusionParityTest, ComplexDiffSumSqDiv) {
    e.eval("z = reshape(linspace(-3,4,6000),2000,3) + "
           "1i*reshape(linspace(2,-5,6000),2000,3); "
           "w = reshape(linspace(1,-1,6000),2000,3) + "
           "1i*reshape(linspace(-2,2,6000),2000,3); cc = 1+1i; d = 2.5;");
    EXPECT_TRUE(sameOnOff(e, "(z - cc) .^ 2"));         // array-scalar sq-diff
    EXPECT_TRUE(sameOnOff(e, "(cc - z) .^ 2"));          // reversed (pow order matters)
    EXPECT_TRUE(sameOnOff(e, "abs(z - cc)"));             // array-scalar abs-diff
    EXPECT_TRUE(sameOnOff(e, "abs(cc - z)"));             // == |z-cc|
    EXPECT_TRUE(sameOnOff(e, "sqrt(z .^ 2 + w .^ 2)"));   // complex sqrt-sumsq
    EXPECT_TRUE(sameOnOff(e, "((z - cc) ./ d) .^ 2"));    // complex sq-div
    EXPECT_TRUE(sameOnOff(e, "exp((cc - z) ./ d)"));      // reversed div (c-z)/d
    EXPECT_TRUE(sameOnOff(e, "sqrt((cc - z) ./ d)"));
    EXPECT_TRUE(sameOnOff(e, "abs((cc - z) ./ d)"));
}

// NaN/Inf for the diff/sumsq/div completion (incl. the reversed-div fold under
// a non-finite z — confirms (c-z)/d == (z-c)/(-d) bit-exact through std::complex
// division, and the diff/pow on Inf components).
TEST_P(FusionParityTest, ComplexDiffSumSqNaNInf) {
    e.eval("re = [linspace(-2,2,5998)'; Inf; -3]; im = [linspace(1,-1,5998)'; 2; Inf]; "
           "z = complex(re, im); "
           "re2 = [linspace(1,-1,5998)'; 1; 2]; im2 = [linspace(-1,1,5998)'; 3; -1]; "
           "w = complex(re2, im2); cc = 1+1i; d = 2;");
    EXPECT_TRUE(sameOnOff(e, "(z - cc) .^ 2"));
    EXPECT_TRUE(sameOnOff(e, "abs(z - cc)"));
    EXPECT_TRUE(sameOnOff(e, "sqrt(z .^ 2 + w .^ 2)"));
    EXPECT_TRUE(sameOnOff(e, "exp((cc - z) ./ d)"));
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

// implicit-1 axpby `x + b.*y` (one product + a same-shape array leaf). Routes
// through affine_add to fusedAxpby; this confirms that routing actually fires.
TEST(FusionFiringProbe, DISABLED_VMAxpbyImplicitOneSpeedup) {
    probeFused(numkit::Engine::Backend::VM, "VM axpby-a1",
               "x = rand(3048*3816,1); y = rand(3048*3816,1); b = 0.7;",
               "x + b .* y");
}
TEST(FusionFiringProbe, DISABLED_VMSqAffineSpeedup) {
    probeFused(numkit::Engine::Backend::VM, "VM sq-affine",
               "x = rand(3048*3816,1); a = 1.5; b = 0.5;", "(a .* x + b) .^ 2");
}
TEST(FusionFiringProbe, DISABLED_VMSqrtSumSqSpeedup) {
    probeFused(numkit::Engine::Backend::VM, "VM sqrt-sumsq",
               "x = rand(3048*3816,1); y = rand(3048*3816,1);", "sqrt(x .^ 2 + y .^ 2)");
}
// clamp of a divide-inner — the rescale-then-saturate `max(0,min(1,(x-lo)./rng))`
// collapses subtract + divide + temporary + clamp into one streaming pass.
TEST(FusionFiringProbe, DISABLED_VMAffineClampDivSpeedup) {
    probeFused(numkit::Engine::Backend::VM, "VM clamp-div",
               "x = rand(3048*3816,1); lo = 0.2; rng = 0.5;",
               "max(0, min(1, (x - lo) ./ rng))");
}
// sq / abs of a divide-inner (squared z-score / normalized abs deviation).
TEST(FusionFiringProbe, DISABLED_VMSqDivSpeedup) {
    probeFused(numkit::Engine::Backend::VM, "VM sq-div",
               "x = rand(3048*3816,1); mu = 0.5; sg = 0.2;", "((x - mu) ./ sg) .^ 2");
}
TEST(FusionFiringProbe, DISABLED_VMAbsDivSpeedup) {
    probeFused(numkit::Engine::Backend::VM, "VM abs-div",
               "x = rand(3048*3816,1); mu = 0.5; sg = 0.2;", "abs((x - mu) ./ sg)");
}
TEST(FusionFiringProbe, DISABLED_VMSoftThresholdSpeedup) {
    probeFused(numkit::Engine::Backend::VM, "VM soft-threshold",
               "x = rand(3048*3816,1) * 10 - 5; t = 1.5;",
               "sign(x) .* max(0, abs(x) - t)");
}
// exp-affine: compute-bound on the polynomial, so the expected win is modest
// (it saves the affine temporary, not the exp cost). 0.92 threshold accordingly.
TEST(FusionFiringProbe, DISABLED_VMExpAffineSpeedup) {
    numkit::StandardEngine e;
    e.setBackend(numkit::Engine::Backend::VM);
    e.eval("x = rand(3048*3816,1); a = -0.5; b = 0.25;");
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval("y = exp(a .* x + b);");  // warm
        const int iters = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval("y = exp(a .* x + b);");
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] VM exp-affine 11.6M: fusion-off %.2f ms, fusion-on "
                "%.2f ms (%.2fx)\n", off, on, off / on);
    EXPECT_LT(on, off * 0.98);  // modest (compute-bound) but it fired
}

// Rare transcendentals are compute-bound (the affine temp is a small share), so
// the win is modest — 0.99 threshold, just enough to prove the kernel fired.
namespace {
void probeTransModest(const char *tag, const char *setup, const char *expr) {
    numkit::StandardEngine e;
    e.setBackend(numkit::Engine::Backend::VM);
    e.eval(setup);
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval(std::string("y = ") + expr + ";");  // warm
        const int iters = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval(std::string("y = ") + expr + ";");
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] %s 11.6M: fusion-off %.2f ms, fusion-on %.2f ms "
                "(%.2fx)\n", tag, off, on, off / on);
    EXPECT_LT(on, off * 0.99);  // compute-bound; fired
}
} // namespace

TEST(FusionFiringProbe, DISABLED_VMCoshAffineSpeedup) {
    probeTransModest("VM cosh-affine", "x = rand(3048*3816,1); a = 0.5; b = 0.1;",
                     "cosh(a .* x + b)");
}
TEST(FusionFiringProbe, DISABLED_VMAsinAffineSpeedup) {
    // affine kept in (-1,1) so it stays real and the kernel runs.
    probeTransModest("VM asin-affine", "x = rand(3048*3816,1)*1.6 - 0.8; a = 0.5; b = 0;",
                     "asin(a .* x + b)");
}
TEST(FusionFiringProbe, DISABLED_VMTanAffineSpeedup) {
    probeTransModest("VM tan-affine", "x = rand(3048*3816,1); a = 1.5; b = 0.2;",
                     "tan(a .* x + b)");
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

// round-affine `round(a.*x+b)` — cheap op, memory-bound, so a clear speedup.
TEST(FusionFiringProbe, DISABLED_VMRoundAffineSpeedup) {
    probeFused(numkit::Engine::Backend::VM, "VM round-affine",
               "x = rand(3048*3816,1)*100 - 50; a = 1.5; b = 0.25;",
               "round(a .* x + b)");
}

// complex affine `ca.*z+cb` — scalar std::complex loop (no SIMD), so the win is
// temp elimination over 16-byte complex elements (memory-bound).
TEST(FusionFiringProbe, DISABLED_VMComplexAffineSpeedup) {
    numkit::StandardEngine e;
    e.setBackend(numkit::Engine::Backend::VM);
    e.eval("z = rand(3048*3816,1) + 1i*rand(3048*3816,1); ca = 1+2i; cb = 0.5-1i;");
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval("y = ca .* z + cb;");  // warm
        const int iters = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval("y = ca .* z + cb;");
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] VM complex-affine 11.6M: fusion-off %.2f ms, fusion-on "
                "%.2f ms (%.2fx)\n", off, on, off / on);
    EXPECT_LT(on, off * 0.95);  // temp-elim win (scalar std::complex, no SIMD)
}

// complex sqrt(ca.*z) — scalar std::complex unary; eliminates the ca.*z temp.
TEST(FusionFiringProbe, DISABLED_VMComplexSqrtSpeedup) {
    numkit::StandardEngine e;
    e.setBackend(numkit::Engine::Backend::VM);
    e.eval("z = rand(3048*3816,1) + 1i*rand(3048*3816,1); ca = 1+2i;");
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval("y = sqrt(ca .* z);");  // warm
        const int iters = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval("y = sqrt(ca .* z);");
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] VM complex-sqrt 11.6M: fusion-off %.2f ms, fusion-on "
                "%.2f ms (%.2fx)\n", off, on, off / on);
    EXPECT_LT(on, off * 0.99);  // temp-elim (compute-bound on std::sqrt(complex))
}

// complex abs `abs(ca.*z)` → real magnitude; eliminates the ca.*z complex temp.
TEST(FusionFiringProbe, DISABLED_VMComplexAbsSpeedup) {
    numkit::StandardEngine e;
    e.setBackend(numkit::Engine::Backend::VM);
    e.eval("z = rand(3048*3816,1) + 1i*rand(3048*3816,1); ca = 1+2i;");
    auto run = [&](bool on) {
        e.setFusion(on);
        e.eval("y = abs(ca .* z);");  // warm
        const int iters = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) e.eval("y = abs(ca .* z);");
        auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    };
    const double off = run(false), on = run(true);
    std::printf("[fusion] VM complex-abs 11.6M: fusion-off %.2f ms, fusion-on "
                "%.2f ms (%.2fx)\n", off, on, off / on);
    EXPECT_LT(on, off * 0.97);  // temp-elim win
}
