// libs/stats/tests/raylinv_test.cpp
// raylinv.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RaylinvTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(RaylinvTest, Median)
{
    EXPECT_NEAR(evalScalar("raylinv(0.5, 1)"), 1.1774100225154747, 1e-12);
}

TEST_F(RaylinvTest, VectorQ)
{
    eval("x = raylinv([0.05 0.5 0.95], 1);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.32029141227185765, 1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 2.4477468306808161, 1e-12);
}

TEST_F(RaylinvTest, BoundaryQuantiles)
{
    EXPECT_DOUBLE_EQ(evalScalar("raylinv(0, 1)"), 0.0);
    EXPECT_TRUE(std::isinf(evalScalar("raylinv(1, 1)")));
}

TEST_F(RaylinvTest, EdgeCases)
{
    EXPECT_TRUE(std::isnan(evalScalar("raylinv(-0.1, 1)")));
    EXPECT_TRUE(std::isnan(evalScalar("raylinv( 1.5, 1)")));
    EXPECT_TRUE(std::isnan(evalScalar("raylinv( 0.5, 0)")));
    EXPECT_TRUE(std::isnan(evalScalar("raylinv( 0.5, -1)")));
}
