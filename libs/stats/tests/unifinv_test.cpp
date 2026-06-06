// libs/stats/tests/unifinv_test.cpp
// unifinv.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class UnifinvTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(UnifinvTest, DefaultsUnitInterval)
{
    EXPECT_DOUBLE_EQ(evalScalar("unifinv(0.5)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("unifinv(0)"),   0.0);
    EXPECT_DOUBLE_EQ(evalScalar("unifinv(1)"),   1.0);
}

TEST_F(UnifinvTest, WiderInterval)
{
    EXPECT_DOUBLE_EQ(evalScalar("unifinv(0.25, 1, 5)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("unifinv(0,    1, 5)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("unifinv(1,    1, 5)"), 5.0);
}

TEST_F(UnifinvTest, VectorQuantile)
{
    eval("v = unifinv([0.05 0.5 0.95], 0, 10);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 9.5);
}

TEST_F(UnifinvTest, ProbOutOfRange)
{
    EXPECT_TRUE(std::isnan(evalScalar("unifinv(-0.1)")));
    EXPECT_TRUE(std::isnan(evalScalar("unifinv( 1.5)")));
    EXPECT_TRUE(std::isnan(evalScalar("unifinv( NaN, 0, 1)")));
}

TEST_F(UnifinvTest, BadParams)
{
    EXPECT_TRUE(std::isnan(evalScalar("unifinv(0.5, 1, 0)")));   // b < a
    EXPECT_TRUE(std::isnan(evalScalar("unifinv(0.5, 5, 1)")));   // b < a
    EXPECT_TRUE(std::isnan(evalScalar("unifinv(0.5, 1, 1)")));   // b == a degenerate
    EXPECT_TRUE(std::isnan(evalScalar("unifinv(0.5, NaN, 1)")));
}
