// toolboxes/stats/tests/gaminv_test.cpp
// gaminv.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GaminvTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(GaminvTest, Median)
{
    EXPECT_NEAR(evalScalar("gaminv(0.5, 2, 1)"), 1.6783469900166605, 1e-9);
}

TEST_F(GaminvTest, VectorQ)
{
    eval("x = gaminv([0.05 0.5 0.95], 2, 1);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.3553615106986621, 1e-9);
    EXPECT_NEAR(evalScalar("x(3)"), 4.7438645183905788, 1e-9);
}

TEST_F(GaminvTest, BoundaryQuantiles)
{
    EXPECT_DOUBLE_EQ(evalScalar("gaminv(0, 2, 1)"), 0.0);
    EXPECT_TRUE(std::isinf(evalScalar("gaminv(1, 2, 1)")));
}

TEST_F(GaminvTest, OutOfRangeProbReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("gaminv(-0.1, 2, 1)")));
    EXPECT_TRUE(std::isnan(evalScalar("gaminv( 1.5, 2, 1)")));
}

TEST_F(GaminvTest, A0DegenerateZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("gaminv(0.5, 0, 1)"), 0.0);  // matches MATLAB
}

TEST_F(GaminvTest, InvalidParamsReturnNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("gaminv(0.5, -1, 1)")));
    EXPECT_TRUE(std::isnan(evalScalar("gaminv(0.5,  2, 0)")));
}
