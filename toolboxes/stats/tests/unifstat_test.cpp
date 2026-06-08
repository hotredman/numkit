// toolboxes/stats/tests/unifstat_test.cpp
// unifstat.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class UnifstatTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(UnifstatTest, ScalarMomentsU01)
{
    eval("[m, v] = unifstat(0, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 0.5);
    EXPECT_NEAR(evalScalar("v"), 1.0/12.0, 1e-12);
}

TEST_F(UnifstatTest, VectorBroadcasting)
{
    eval("[m, v] = unifstat([0 -2], [1 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 1.5);
}

TEST_F(UnifstatTest, ScalarAVectorB)
{
    eval("[m, v] = unifstat(0, [1 2 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"), 2.5);
}

TEST_F(UnifstatTest, DegenerateOrInvertedReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("unifstat(1, 1)")));   // a == b → NaN
    EXPECT_TRUE(std::isnan(evalScalar("unifstat(2, 1)")));   // a > b  → NaN
}
