// libs/stats/tests/descriptive_six_test.cpp
//
// Regression guard for range / mad / geomean / harmmean / moment / trimmean.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DescriptiveSixTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(DescriptiveSixTest, RangeVector)
{
    EXPECT_DOUBLE_EQ(evalScalar("range([1 5 3 8 2 7 4 6])"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("range([3 3 3])"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("range([-5 5])"), 10.0);
}

TEST_F(DescriptiveSixTest, RangeMatrixDim)
{
    eval("A = [1 2 3; 4 5 6; 7 8 9]; r1 = range(A); r2 = range(A, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("r1(1)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("r1(2)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("r2(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("r2(2)"), 2.0);
}

TEST_F(DescriptiveSixTest, MadMeanForm)
{
    // mean = 4.5; mean(|x - 4.5|) = 2.0.
    EXPECT_DOUBLE_EQ(evalScalar("mad([1 5 3 8 2 7 4 6])"), 2.0);
}

TEST_F(DescriptiveSixTest, MadMedianForm)
{
    // median = 4.5; median(|x - 4.5|) = 2.0.
    EXPECT_DOUBLE_EQ(evalScalar("mad([1 5 3 8 2 7 4 6], 1)"), 2.0);
}

TEST_F(DescriptiveSixTest, GeomeanInteger)
{
    EXPECT_DOUBLE_EQ(evalScalar("geomean([2 4 8])"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("geomean([1 1 1])"), 1.0);
}

TEST_F(DescriptiveSixTest, GeomeanWithZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("geomean([0 1 2])"), 0.0);
}

TEST_F(DescriptiveSixTest, HarmmeanBasic)
{
    EXPECT_NEAR(evalScalar("harmmean([1 2 4])"), 12.0/7.0, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("harmmean([1 1 1])"), 1.0);
}

TEST_F(DescriptiveSixTest, MomentSecondIsPopVariance)
{
    // moment(x, 2) = (1/n) * sum((x - mean(x))^2). Population variance.
    eval("v = [1 5 3 8 2 7 4 6]; m2 = moment(v, 2);");
    EXPECT_NEAR(evalScalar("m2"), 5.25, 1e-12);
}

TEST_F(DescriptiveSixTest, MomentEvenZeroForSymmetric)
{
    // Symmetric distribution: third central moment = 0.
    EXPECT_DOUBLE_EQ(evalScalar("moment([-2 -1 0 1 2], 3)"), 0.0);
}

TEST_F(DescriptiveSixTest, TrimmeanBasic)
{
    // Trim 25% (12.5% from each end) of 9 values: floor(9 * 25/200) = floor(1.125) = 1.
    // Sorted: 1 2 3 4 5 6 7 8 100. Trim 1 from each end -> 2..8.
    // mean(2..8) = 5.
    EXPECT_DOUBLE_EQ(evalScalar("trimmean([1 5 3 8 2 7 4 6 100], 25)"), 5.0);
}

TEST_F(DescriptiveSixTest, TrimmeanZeroPctIsMean)
{
    // 0% trim = regular mean.
    EXPECT_NEAR(evalScalar("trimmean([1 2 3 4 5], 0)"), 3.0, 1e-12);
}
