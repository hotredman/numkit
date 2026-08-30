// tests/resample_test.cpp

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

class ResampleTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// ============================================================
// downsample
// ============================================================

TEST_F(ResampleTest, DownsampleByTwo)
{
    eval("y = downsample([1 2 3 4 5 6], 2);");
    EXPECT_EQ(eval("y").numel(), 3u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 5.0);
}

TEST_F(ResampleTest, DownsampleByThree)
{
    eval("y = downsample([1 2 3 4 5 6 7 8 9], 3);");
    EXPECT_EQ(eval("y").numel(), 3u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 7.0);
}

TEST_F(ResampleTest, DownsampleByOne)
{
    eval("y = downsample([1 2 3], 1);");
    EXPECT_EQ(eval("y").numel(), 3u);
}

// ============================================================
// upsample
// ============================================================

TEST_F(ResampleTest, UpsampleByTwo)
{
    eval("y = upsample([1 2 3], 2);");
    EXPECT_EQ(eval("y").numel(), 6u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(6)"), 0.0);
}

TEST_F(ResampleTest, UpsampleByThree)
{
    eval("y = upsample([1 2], 3);");
    EXPECT_EQ(eval("y").numel(), 6u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 2.0);
}

// ============================================================
// decimate
// ============================================================

TEST_F(ResampleTest, DecimateReducesLength)
{
    eval("y = decimate(ones(1, 100), 4);");
    EXPECT_EQ(eval("y").numel(), 25u);
}

TEST_F(ResampleTest, DecimatePreservesDc)
{
    // Constant signal should survive decimation
    eval("y = decimate(5 * ones(1, 200), 4);");
    // After filter settles, output ≈ 5
    double lastVal = evalScalar("y(25)");
    EXPECT_NEAR(lastVal, 5.0, 0.5);
}

TEST_F(ResampleTest, DecimateAntiAliases)
{
    // High frequency signal should be attenuated
    eval("n = 0:199;");
    eval("x = cos(0.9 * pi * n);"); // near Nyquist
    eval("y = decimate(x, 4);");
    // Decimated signal should have much less energy than original per sample
    double origPower = evalScalar("sum(x.^2) / length(x)");
    double decPower = evalScalar("sum(y.^2) / length(y)");
    EXPECT_LT(decPower, origPower * 0.5);
}

// ============================================================
// resample
// ============================================================

TEST_F(ResampleTest, ResampleUpsample)
{
    // resample(x, 2, 1) → doubles the rate
    eval("y = resample([1 2 3 4], 2, 1);");
    EXPECT_EQ(eval("y").numel(), 8u);
}

TEST_F(ResampleTest, ResampleDownsample)
{
    // resample(x, 1, 2) → halves the rate
    eval("y = resample(ones(1, 100), 1, 2);");
    EXPECT_EQ(eval("y").numel(), 50u);
}

TEST_F(ResampleTest, ResampleRational)
{
    // resample(x, 3, 2) → rate * 3/2
    eval("x = ones(1, 100);");
    eval("y = resample(x, 3, 2);");
    EXPECT_EQ(eval("y").numel(), 150u);
}

// MATLAB R2025b parity: exact values for a 3/2 ramp (the bug repro).
TEST_F(ResampleTest, ResampleRationalValues)
{
    eval("y = resample([1 2 3 4 5 6], 3, 2);");
    EXPECT_EQ(eval("y").numel(), 9u);
    EXPECT_NEAR(evalScalar("y(1)"), 1.0006061736, 1e-9);
    EXPECT_NEAR(evalScalar("y(5)"), 3.9409926893, 1e-9);
    EXPECT_NEAR(evalScalar("y(9)"), 4.2402907078, 1e-9);
    EXPECT_NEAR(evalScalar("sum(y)"), 31.6965, 1e-3);
}

// A DC level survives resampling (settled interior ≈ input level).
TEST_F(ResampleTest, ResamplePreservesDc)
{
    eval("y = resample(5 * ones(1, 100), 3, 2);");
    EXPECT_NEAR(evalScalar("y(75)"), 4.9984845661, 1e-7);
}

// Column input → column output (orientation preserved).
TEST_F(ResampleTest, ResampleColumnOrientation)
{
    eval("y = resample((1:6)', 3, 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y,1)")), 9);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y,2)")), 1);
}

// GCD reduction: resample(x, 4, 2) == resample(x, 2, 1).
TEST_F(ResampleTest, ResampleGcdReduction)
{
    eval("a = resample([1 2 3 4 5 6], 4, 2); b = resample([1 2 3 4 5 6], 2, 1);");
    EXPECT_LT(evalScalar("max(abs(a - b))"), 1e-12);
}
