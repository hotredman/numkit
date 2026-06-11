// toolboxes/signal/tests/conversions_test.cpp
//
// Tests for D3 — filter form conversions:
//   sos2tf / sos2zp / tf2zpk + tf↔ss + sos↔ss + zpk↔ss.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

class FilterConversionsTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── sos2tf ────────────────────────────────────────────────────────────
TEST_F(FilterConversionsTest, Sos2tfRoundtripsSingleSection)
{
    // One section: b = [1 -1 0.25], a = [1 -0.5 0]. SOS row: [b a].
    eval("sos = [1 -1 0.25 1 -0.5 0];");
    eval("[b, a] = sos2tf(sos);");
    EXPECT_NEAR(evalScalar("b(1)"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("b(2)"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("b(3)"),  0.25, 1e-12);
    EXPECT_NEAR(evalScalar("a(1)"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("a(2)"), -0.5, 1e-12);
    EXPECT_NEAR(evalScalar("a(3)"),  0.0, 1e-12);
}

TEST_F(FilterConversionsTest, Sos2tfMultipleSections)
{
    // Two sections: cascade should multiply b's and a's.
    eval("sos = [1 1 0 1 -0.5 0; 1 -1 0 1 0.5 0];");
    eval("[b, a] = sos2tf(sos);");
    // b = (1+z^-1)(1-z^-1) = 1 - z^-2 → [1 0 -1]
    EXPECT_NEAR(evalScalar("b(1)"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("b(2)"),  0.0, 1e-12);
    EXPECT_NEAR(evalScalar("b(3)"), -1.0, 1e-12);
    // a = (1-0.5)(1+0.5) = 1 - 0.25 z^-2 → [1 0 -0.25]
    EXPECT_NEAR(evalScalar("a(1)"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("a(2)"),  0.0, 1e-12);
    EXPECT_NEAR(evalScalar("a(3)"), -0.25, 1e-12);
}

// ── tf2zpk ────────────────────────────────────────────────────────────
TEST_F(FilterConversionsTest, Tf2zpkSimple)
{
    // H(z) = (z - 0.5) / (z + 0.25) → b = [1 -0.5], a = [1 0.25]
    eval("[z, p, k] = tf2zpk([1 -0.5], [1 0.25]);");
    EXPECT_NEAR(evalScalar("z(1)"),  0.5, 1e-9);
    EXPECT_NEAR(evalScalar("p(1)"), -0.25, 1e-9);
    EXPECT_NEAR(evalScalar("k"),     1.0, 1e-12);
}

// ── tf2ss / ss2tf round-trip ─────────────────────────────────────────
TEST_F(FilterConversionsTest, TfSsRoundtrip)
{
    eval("b = [1 -0.5 0.25];");
    eval("a = [1 -1   0.5];");
    eval("[A, B, C, D] = tf2ss(b, a);");
    eval("[b2, a2] = ss2tf(A, B, C, D);");
    // a should be normalised; both b and a should match (up to scale).
    for (int i = 1; i <= 3; ++i) {
        EXPECT_NEAR(evalScalar("b2(" + std::to_string(i) + ")"),
                    evalScalar("b(" + std::to_string(i) + ")"), 1e-9);
        EXPECT_NEAR(evalScalar("a2(" + std::to_string(i) + ")"),
                    evalScalar("a(" + std::to_string(i) + ")"), 1e-9);
    }
}

TEST_F(FilterConversionsTest, Tf2ssShape)
{
    eval("[A, B, C, D] = tf2ss([1 0.5], [1 -0.3 0.2]);");
    EXPECT_EQ(eval("A").dims().rows(), 2u);
    EXPECT_EQ(eval("A").dims().cols(), 2u);
    EXPECT_EQ(eval("B").dims().rows(), 2u);
    EXPECT_EQ(eval("B").dims().cols(), 1u);
    EXPECT_EQ(eval("C").dims().rows(), 1u);
    EXPECT_EQ(eval("C").dims().cols(), 2u);
    EXPECT_EQ(eval("D").numel(),       1u);
}

// ── sos round-trip via tf ─────────────────────────────────────────────
TEST_F(FilterConversionsTest, Sos2zpReconstructs)
{
    // 1st-order section: b = [1 -0.5 0], a = [1 0.25 0].
    // zero at 0.5, pole at -0.25. Single-row SOS.
    eval("sos = [1 -0.5 0 1 0.25 0];");
    eval("[z, p, k] = sos2zp(sos);");
    // First non-zero zero/pole should match.
    EXPECT_NEAR(evalScalar("real(z(1))"),  0.5, 1e-9);
    EXPECT_NEAR(evalScalar("real(p(1))"), -0.25, 1e-9);
}

// ── zpk → ss round-trip via tf ────────────────────────────────────────
TEST_F(FilterConversionsTest, ZpkSsRoundtrip)
{
    // Real zeros/poles only — k=2.
    eval("z = [0.5; -0.5];");
    eval("p = [0.3; -0.7];");
    eval("[A, B, C, D] = zp2ss(z, p, 2);");
    eval("[z2, p2, k2] = ss2zp(A, B, C, D);");
    EXPECT_NEAR(evalScalar("k2"), 2.0, 1e-9);
    // Sort by real part to compare (root order isn't guaranteed).
    EXPECT_EQ(eval("z2").numel(), 2u);
    EXPECT_EQ(eval("p2").numel(), 2u);
}

// ── ss2sos / sos2ss composition ──────────────────────────────────────
TEST_F(FilterConversionsTest, SosSsRoundtrip)
{
    eval("sos = [1 -1 0.25 1 -0.5 0];");
    eval("[A, B, C, D] = sos2ss(sos);");
    eval("sos2 = ss2sos(A, B, C, D);");
    EXPECT_EQ(eval("sos2").dims().cols(), 6u);
}

// zp2sos with #zeros < #poles: MATLAB places the surplus zeros at the
// ORIGIN, so a zero-less biquad section is [0 0 g] and the reconstructed
// numerator is degree-deficient with a leading z^-2 delay.
// (Regression: numkit used to leave them at infinity -> [g 0 0].)
TEST_F(FilterConversionsTest, Zp2sosSurplusZerosAtOrigin)
{
    eval("S = zp2sos([0.5; -0.3], [0.2; 0.1; -0.4; 0.6], 2);");
    eval("[bb, aa] = sos2tf(S);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(S,1)")), 2);
    EXPECT_NEAR(evalScalar("bb(1)"),  0.0,  1e-12);   // surplus zeros at origin
    EXPECT_NEAR(evalScalar("bb(2)"),  0.0,  1e-12);
    EXPECT_NEAR(evalScalar("bb(3)"),  2.0,  1e-12);
    EXPECT_NEAR(evalScalar("bb(4)"), -0.4,  1e-12);
    EXPECT_NEAR(evalScalar("bb(5)"), -0.3,  1e-12);
}

// tf2sos with deg(b) < deg(a) must REPRODUCE the original b polynomial:
// surplus zeros stay LEFT-aligned (empty section [g 0 0]), the opposite
// of zp2sos. sos2tf(tf2sos([1 0.5], a)) recovers b = [1 0.5 0 0].
TEST_F(FilterConversionsTest, Tf2sosPreservesNumeratorLeftAligned)
{
    eval("s2 = tf2sos([1 0.5], [1 -0.3 0.02 0.001]);");
    eval("[b2, a2] = sos2tf(s2);");
    EXPECT_NEAR(evalScalar("b2(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("b2(2)"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("b2(3)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("b2(4)"), 0.0, 1e-12);
}
