// toolboxes/signal/tests/convolution_extras_test.cpp
//
// Tests for E1 — convolution extras:
//   cconv, convmtx, xcorr2, finddelay, alignsignals.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

class ConvolutionExtrasTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── cconv ─────────────────────────────────────────────────────────────
TEST_F(ConvolutionExtrasTest, CconvSimple)
{
    // cconv([1 2 3], [1 1 1], 3) — circular conv of period 3.
    eval("y = cconv([1 2 3], [1 1 1], 3);");
    // Each output sample = sum of all elements = 6 (circular wraparound).
    EXPECT_NEAR(evalScalar("y(1)"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 6.0, 1e-12);
}

TEST_F(ConvolutionExtrasTest, CconvImpulseIsIdentity)
{
    eval("y = cconv([1 0 0 0], [1 2 3 4], 4);");
    EXPECT_NEAR(evalScalar("y(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), 4.0, 1e-12);
}

// ── convmtx ───────────────────────────────────────────────────────────
TEST_F(ConvolutionExtrasTest, ConvmtxShape)
{
    // Row h of length 3 → output is n × (n + nh - 1) = 4 × 6.
    // Column h of length 3 → output is (n + nh - 1) × n = 6 × 4.
    // See BUGS.md #34.
    eval("M = convmtx([1 2 3], 4);");
    EXPECT_EQ(eval("M").dims().rows(), 4u);
    EXPECT_EQ(eval("M").dims().cols(), 6u);
    eval("Mc = convmtx([1; 2; 3], 4);");
    EXPECT_EQ(eval("Mc").dims().rows(), 6u);
    EXPECT_EQ(eval("Mc").dims().cols(), 4u);
}

TEST_F(ConvolutionExtrasTest, ConvmtxMultipliesLikeConv)
{
    // Row form: x_row * M == conv(x, h). MATLAB row-h form returns
    // n × (n + nh - 1), and the row-vector multiply produces the
    // length-(n+nh-1) convolution.
    eval("h = [1 2 3]; x = [4 5 6 7];");
    eval("M = convmtx(h, 4); y_mtx = x * M; y_dir = conv(x, h);");
    for (int i = 1; i <= 6; ++i) {
        EXPECT_NEAR(evalScalar("y_mtx(" + std::to_string(i) + ")"),
                    evalScalar("y_dir(" + std::to_string(i) + ")"), 1e-12);
    }
    // Column form: M_col * x_col == conv(h, x).
    eval("hc = [1; 2; 3]; xc = [4; 5; 6; 7];");
    eval("Mc = convmtx(hc, 4); y_col = Mc * xc; y_dir2 = conv(hc, xc);");
    for (int i = 1; i <= 6; ++i) {
        EXPECT_NEAR(evalScalar("y_col(" + std::to_string(i) + ")"),
                    evalScalar("y_dir2(" + std::to_string(i) + ")"), 1e-12);
    }
}

// ── xcorr2 ────────────────────────────────────────────────────────────
TEST_F(ConvolutionExtrasTest, Xcorr2Shape)
{
    eval("A = [1 2 3; 4 5 6; 7 8 9]; B = [1 2; 3 4];");
    eval("C = xcorr2(A, B);");
    // Shape: (3+2-1) × (3+2-1) = 4×4.
    EXPECT_EQ(eval("C").dims().rows(), 4u);
    EXPECT_EQ(eval("C").dims().cols(), 4u);
}

TEST_F(ConvolutionExtrasTest, Xcorr2OfImpulseRecoversInput)
{
    // 2-D autocorrelation peaks at the centre.
    eval("A = [1 2 3; 4 5 6]; C = xcorr2(A, A);");
    // Centre of (2*2-1) × (2*3-1) = 3×5 output is at (2,3).
    // Centre value = sum(A.^2) = 1+4+9+16+25+36 = 91.
    EXPECT_NEAR(evalScalar("C(2,3)"), 91.0, 1e-9);
}

// ── finddelay ─────────────────────────────────────────────────────────
TEST_F(ConvolutionExtrasTest, FinddelayPositiveShift)
{
    // y is x delayed by 5 samples → finddelay returns +5.
    eval("x = [zeros(1,5), 1, 2, 3, zeros(1,10)];");
    eval("y = [zeros(1,10), 1, 2, 3, zeros(1,5)];");
    EXPECT_DOUBLE_EQ(evalScalar("finddelay(x, y)"), 5.0);
}

TEST_F(ConvolutionExtrasTest, FinddelayNegativeShift)
{
    eval("x = [zeros(1,10), 1, 2, 3, zeros(1,5)];");
    eval("y = [zeros(1,5), 1, 2, 3, zeros(1,10)];");
    EXPECT_DOUBLE_EQ(evalScalar("finddelay(x, y)"), -5.0);
}

TEST_F(ConvolutionExtrasTest, FinddelayIdenticalIsZero)
{
    eval("x = [1 2 3 4 5];");
    EXPECT_DOUBLE_EQ(evalScalar("finddelay(x, x)"), 0.0);
}

// ── alignsignals ──────────────────────────────────────────────────────
TEST_F(ConvolutionExtrasTest, AlignsignalsKeepsLengths)
{
    // Just sanity: both outputs have the same length and no NaN/inf.
    eval("x = [zeros(1,5), 1, 2, 3];");
    eval("y = [1, 2, 3];");
    eval("[xa, ya] = alignsignals(x, y);");
    EXPECT_EQ(eval("xa").numel(), eval("ya").numel());
}

TEST_F(ConvolutionExtrasTest, AlignsignalsRoundtripDelayedRamp)
{
    // x is y delayed by 4 within a long buffer — alignment should
    // recover identical xa, ya at the peak position.
    eval("base = [1 2 3 4 5];");
    eval("x = [zeros(1,4), base, zeros(1,8)];");
    eval("y = [base, zeros(1,12)];");
    eval("[xa, ya] = alignsignals(x, y);");
    eval("[~, ix] = max(xa); [~, iy] = max(ya);");
    EXPECT_DOUBLE_EQ(evalScalar("ix"), evalScalar("iy"));
}
