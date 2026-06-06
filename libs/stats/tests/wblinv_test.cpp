// libs/stats/tests/wblinv_test.cpp
// wblinv.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WblinvTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(WblinvTest, DefaultsExponentialEquivalent)
{
    // Default a=1, b=1 -> exponential, x = -log(1-p).
    EXPECT_NEAR(evalScalar("wblinv(0.5)"),  0.6931471805599453, 1e-12);
    EXPECT_NEAR(evalScalar("wblinv(0.05)"), 0.0512932943875506, 1e-12);
    EXPECT_NEAR(evalScalar("wblinv(0.95)"), 2.9957322735539900, 1e-12);
}

TEST_F(WblinvTest, BoundaryProb)
{
    EXPECT_DOUBLE_EQ(evalScalar("wblinv(0)"), 0.0);
    EXPECT_TRUE(std::isinf(evalScalar("wblinv(1)")));
    EXPECT_GT(evalScalar("wblinv(1)"), 0.0);
}

TEST_F(WblinvTest, WithScaleAndShape)
{
    EXPECT_NEAR(evalScalar("wblinv(0.5, 2, 3)"),    1.7699940890010355, 1e-12);
    EXPECT_NEAR(evalScalar("wblinv(0.5, 1, 0.5)"),  0.4804530139182014, 1e-12);
    EXPECT_NEAR(evalScalar("wblinv(0.5, 1, 2)"),    0.8325546111576977, 1e-12);
}

TEST_F(WblinvTest, VectorQuantile)
{
    eval("v = wblinv([0.05 0.5 0.95], 1, 2);");
    EXPECT_NEAR(evalScalar("v(1)"), 0.2264802295732467, 1e-12);
    EXPECT_NEAR(evalScalar("v(2)"), 0.8325546111576977, 1e-12);
    EXPECT_NEAR(evalScalar("v(3)"), 1.7308183826022849, 1e-12);
}

TEST_F(WblinvTest, ProbOutOfRange)
{
    EXPECT_TRUE(std::isnan(evalScalar("wblinv(-0.1)")));
    EXPECT_TRUE(std::isnan(evalScalar("wblinv( 1.5)")));
    EXPECT_TRUE(std::isnan(evalScalar("wblinv( NaN, 1, 1)")));
}

TEST_F(WblinvTest, BadParams)
{
    EXPECT_TRUE(std::isnan(evalScalar("wblinv(0.5,  0, 1)")));
    EXPECT_TRUE(std::isnan(evalScalar("wblinv(0.5, -1, 1)")));
    EXPECT_TRUE(std::isnan(evalScalar("wblinv(0.5,  1, 0)")));
    EXPECT_TRUE(std::isnan(evalScalar("wblinv(0.5,  1, -1)")));
    EXPECT_TRUE(std::isnan(evalScalar("wblinv(0.5,  NaN, 1)")));
}
