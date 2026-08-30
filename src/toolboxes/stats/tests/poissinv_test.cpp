// toolboxes/stats/tests/poissinv_test.cpp
// poissinv.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PoissinvTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(PoissinvTest, Median)
{
    EXPECT_DOUBLE_EQ(evalScalar("poissinv(0.5, 2)"), 2.0);
}

TEST_F(PoissinvTest, VectorQ)
{
    eval("x = poissinv([0.05 0.5 0.95], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(3)"), 5.0);
}

TEST_F(PoissinvTest, BoundaryQuantiles)
{
    EXPECT_DOUBLE_EQ(evalScalar("poissinv(0, 2)"), 0.0);
    EXPECT_TRUE(std::isinf(evalScalar("poissinv(1, 2)")));
}

TEST_F(PoissinvTest, OutOfRangeProbReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("poissinv(-0.1, 2)")));
    EXPECT_TRUE(std::isnan(evalScalar("poissinv( 1.5, 2)")));
}

TEST_F(PoissinvTest, Lambda0Degenerate)
{
    EXPECT_DOUBLE_EQ(evalScalar("poissinv(0.5, 0)"), 0.0);  // matches MATLAB
}

TEST_F(PoissinvTest, NegativeLambdaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("poissinv(0.5, -1)")));
}
