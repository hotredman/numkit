// tests/resample_test.cpp

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/builtin/Library.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit::m;

class ResampleTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { StdLibrary::install(engine); }
    MValue eval(const std::string &code) { return engine.eval(code); }
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
