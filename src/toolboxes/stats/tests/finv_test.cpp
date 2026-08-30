// toolboxes/stats/tests/finv_test.cpp
// finv. Reference values from MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FinvTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(FinvTest, F11Quantiles)
{
    // F(1,1) is heavy-tailed — 95% quantile is the famous 161.45.
    eval("x = finv([0.05 0.5 0.95], 1, 1);");
    EXPECT_NEAR(evalScalar("x(1)"),   0.0061939586571082, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"),   1.0,                1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 161.4476387975890646, 1e-9);
}

TEST_F(FinvTest, F5_10Quantiles)
{
    eval("x = finv([0.05 0.5 0.95], 5, 10);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.2111904287823449, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 0.9319331608510454, 1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 3.3258345304130104, 1e-12);
}

TEST_F(FinvTest, F10_30Quantiles)
{
    eval("x = finv([0.05 0.5 0.95], 10, 30);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.3704319398594768, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 0.9553965764236254, 1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 2.1645799171254732, 1e-12);
}

TEST_F(FinvTest, BoundaryProbabilities)
{
    EXPECT_DOUBLE_EQ(evalScalar("finv(0.0, 5, 10)"), 0.0);
    EXPECT_TRUE(std::isinf(evalScalar("finv(1.0, 5, 10)")));
}

TEST_F(FinvTest, OutOfRangeProbReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("finv(-0.1, 5, 10)")));
    EXPECT_TRUE(std::isnan(evalScalar("finv( 1.5, 5, 10)")));
}

TEST_F(FinvTest, InvalidDofReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("finv(0.5,  0, 10)")));
    EXPECT_TRUE(std::isnan(evalScalar("finv(0.5,  5,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("finv(0.5, -1, 10)")));
}
