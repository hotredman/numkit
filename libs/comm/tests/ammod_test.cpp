// libs/comm/tests/ammod_test.cpp
//
// Regression guard for ammod() — amplitude modulator.
// y = (x + carr_amp) .* cos(2π·Fc·t + ini_phase)

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class AmmodTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(AmmodTest, DSBSCKnownValues)
{
    // Reference (full precision from MATLAB R2025b):
    //   y = ammod(sin(2*pi*5*((0:9)'/8000)), 100, 8000)
    //   y(1) = 0
    //   y(2) = 0.003914875152798067
    //   y(10) = 0.02686937052876025
    eval("fs = 8000; fc = 100; t = (0:9)'/fs; x = sin(2*pi*5*t);"
         "y = ammod(x, fc, fs);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    // Highway sin contributes a few ULP -> ammod via cos*sin similar.
    EXPECT_NEAR(evalScalar("y(2)"),  0.003914875152798067, 1e-9);
    EXPECT_NEAR(evalScalar("y(10)"), 0.02686937052876025,  1e-9);
}

TEST_F(AmmodTest, DSBTCWithCarrAmpAndPhase)
{
    // y2 = ammod(sin(2*pi*5*t), 100, 8000, pi/4, 0.5)
    // y2(1) = (0 + 0.5) * cos(pi/4) = 0.3535533905932738
    eval("y2 = ammod(sin(2*pi*5*((0:9)'/8000)), 100, 8000, pi/4, 0.5);");
    EXPECT_NEAR(evalScalar("y2(1)"),  0.3535533905932738, 1e-12);
    EXPECT_NEAR(evalScalar("y2(10)"), 0.04200194393895778, 1e-9);
}

TEST_F(AmmodTest, DSBSCAtZeroIsZero)
{
    // x(1) = sin(0) = 0, carramp = 0  -> y(1) = 0 exactly
    EXPECT_DOUBLE_EQ(evalScalar("y = ammod(0, 100, 8000); y(1)"), 0.0);
}

TEST_F(AmmodTest, RowOrientationPreserved)
{
    eval("y = ammod(sin(2*pi*5*((0:9)/8000)), 100, 8000);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 10);
}

TEST_F(AmmodTest, ColumnOrientationPreserved)
{
    eval("y = ammod(sin(2*pi*5*((0:9)'/8000)), 100, 8000);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 10);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 1);
}

TEST_F(AmmodTest, RejectsFsBelowNyquist)
{
    bool threw = false;
    try { eval("ammod(sin((0:9)'/100), 100, 100);"); }
    catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(AmmodTest, RejectsNonPositiveFs)
{
    bool threw = false;
    try { eval("ammod(sin((0:9)'/8000), 100, -1);"); }
    catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(AmmodTest, CarrAmpOnlyShiftsModulation)
{
    // Diff between DSB-TC and DSB-SC at same x is carramp*cos(2pi*fc*t+ini)
    eval("x = sin(2*pi*5*((0:9)'/8000));"
         "y_sc = ammod(x, 100, 8000);"
         "y_tc = ammod(x, 100, 8000, 0, 1);"
         "diff_max = max(abs((y_tc - y_sc) - cos(2*pi*100*(0:9)'/8000)));");
    EXPECT_LT(evalScalar("diff_max"), 1e-12);
}
