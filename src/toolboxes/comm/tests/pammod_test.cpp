// toolboxes/comm/tests/pammod_test.cpp
//
// Regression guard for pammod() / pamdemod() — M-PAM modulation.
// MATLAB R2025b: pammod/pamdemod DEFAULT to 'bin' (binary) symbol
// ordering, NOT 'gray' (unlike qammod, which defaults to 'gray').
// numkit previously defaulted all four to 'gray'.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PammodTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(PammodTest, DefaultIsBinaryNotGray)
{
    // Default order = 'bin': symbol k -> 2k-(M-1).
    eval("yb = real(pammod([0 1 2 3], 4));");
    EXPECT_DOUBLE_EQ(evalScalar("yb(1)"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("yb(2)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("yb(3)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("yb(4)"),  3.0);
}

TEST_F(PammodTest, ExplicitGrayReorders)
{
    // 'gray' maps via gray code -> symbols 2,3 swap to points 3,1.
    eval("yg = real(pammod([0 1 2 3], 4, 0, 'gray'));");
    EXPECT_DOUBLE_EQ(evalScalar("yg(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("yg(4)"), 1.0);
}

TEST_F(PammodTest, M8Binary)
{
    eval("y8 = real(pammod(0:7, 8));");
    EXPECT_DOUBLE_EQ(evalScalar("y8(1)"), -7.0);
    EXPECT_DOUBLE_EQ(evalScalar("y8(2)"), -5.0);
    EXPECT_DOUBLE_EQ(evalScalar("y8(8)"),  7.0);
}

TEST_F(PammodTest, RoundTripBinary)
{
    eval("x = pamdemod(pammod([0 1 2 3], 4), 4);");
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(4)"), 3.0);
}

// ── qammod / qamdemod square-QAM constellation (vs MATLAB R2025b) ──
// MATLAB layout: I increases left→right, Q decreases top→bottom; the
// symbol maps column-major (col = s/sqrt(M), row = s mod sqrt(M)).

TEST_F(PammodTest, Qam4Constellation)
{
    // qammod(0:3,4) = [-1+1i, -1-1i, 1+1i, 1-1i].
    eval("y = qammod(0:3, 4); yr = real(y); yi = imag(y);");
    EXPECT_DOUBLE_EQ(evalScalar("yr(1)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("yi(1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("yr(2)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("yi(2)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("yr(4)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("yi(4)"), -1.0);
}

TEST_F(PammodTest, Qam16FirstColumn)
{
    // First 4 symbols share I=-3; Q = [3 1 -3 -1] (Gray, top→bottom).
    eval("y = qammod(0:15, 16); yr = real(y); yi = imag(y);");
    EXPECT_DOUBLE_EQ(evalScalar("yr(1)"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("yi(1)"),  3.0);
    EXPECT_DOUBLE_EQ(evalScalar("yi(2)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("yi(3)"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("yi(4)"), -1.0);
}

TEST_F(PammodTest, Qam8RectangularConstellation)
{
    // 8-QAM (KI=4, KQ=2): real=[-3 -3 -1 -1 3 3 1 1], imag=[1 -1 ...].
    eval("y = qammod(0:7, 8); yr = real(y); yi = imag(y);");
    EXPECT_DOUBLE_EQ(evalScalar("yr(1)"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("yr(5)"),  3.0);
    EXPECT_DOUBLE_EQ(evalScalar("yr(7)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("yi(2)"), -1.0);
}

TEST_F(PammodTest, QamUnitAveragePower)
{
    // 4-QAM unit-average-power scale = 1/sqrt(2).
    eval("y = qammod(0:3, 4, 'UnitAveragePower', true); yr = real(y);");
    EXPECT_NEAR(evalScalar("yr(1)"), -0.70710678118654746, 1e-12);
}

TEST_F(PammodTest, QamRoundTrip)
{
    eval("z = qamdemod(qammod(0:15, 16), 16);");
    for (int k = 0; k < 16; ++k)
        EXPECT_DOUBLE_EQ(evalScalar("z(" + std::to_string(k + 1) + ")"),
                         static_cast<double>(k));
}
