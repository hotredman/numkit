// toolboxes/stats/tests/norminv_test.cpp
// norminv. Reference values from MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class NorminvTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(NorminvTest, StandardNormalQuantiles)
{
    eval("x = norminv([0.025 0.5 0.975]);");
    EXPECT_NEAR(evalScalar("x(1)"), -1.9599639845393952, 1e-12);  // 95% CI lower
    EXPECT_DOUBLE_EQ(evalScalar("x(2)"),                  0.0);
    EXPECT_NEAR(evalScalar("x(3)"),  1.9599639845393952, 1e-12);  // 95% CI upper
}

TEST_F(NorminvTest, NonZeroMean)
{
    eval("x = norminv([0.025 0.5 0.975], 5, 1);");
    EXPECT_NEAR(evalScalar("x(1)"), 3.0400360154606050, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("x(2)"), 5.0);
    EXPECT_NEAR(evalScalar("x(3)"), 6.9599639845393950, 1e-12);
}

TEST_F(NorminvTest, NonUnitSigma)
{
    eval("x = norminv([0.025 0.5 0.975], 0, 2);");
    EXPECT_NEAR(evalScalar("x(1)"), -3.9199279690787905, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("x(2)"),                   0.0);
    EXPECT_NEAR(evalScalar("x(3)"),  3.9199279690787905, 1e-12);
}

TEST_F(NorminvTest, BoundaryProbabilities)
{
    EXPECT_TRUE(std::isinf(evalScalar("norminv(0)")) && evalScalar("norminv(0)") < 0);
    EXPECT_TRUE(std::isinf(evalScalar("norminv(1)")) && evalScalar("norminv(1)") > 0);
}

TEST_F(NorminvTest, OutOfRangeProbReturnsNaN)
{
    // MATLAB: NaN (not -Inf/+Inf for p<0/p>1).
    EXPECT_TRUE(std::isnan(evalScalar("norminv(-0.1)")));
    EXPECT_TRUE(std::isnan(evalScalar("norminv( 1.5)")));
}

TEST_F(NorminvTest, InvalidSigmaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("norminv(0.5, 0,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("norminv(0.5, 0, -1)")));
}
