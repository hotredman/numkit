// toolboxes/stats/tests/gamstat_test.cpp
// gamstat.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GamstatTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(GamstatTest, ScalarMomentsAreShapeScale)
{
    eval("[m, v] = gamstat(2, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 2.0);   // a·b
    EXPECT_DOUBLE_EQ(evalScalar("v"), 2.0);   // a·b²
}

TEST_F(GamstatTest, VectorBroadcasting)
{
    eval("[m, v] = gamstat([2 5 10], [1 2 0.5]);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"),  2.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 20.0);
}

TEST_F(GamstatTest, ScalarAVectorB)
{
    eval("[m, v] = gamstat(2, [0.5 1 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"), 4.0);
}

TEST_F(GamstatTest, InvalidParamsReturnNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("gamstat(0,  1)")));
    EXPECT_TRUE(std::isnan(evalScalar("gamstat(2,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("gamstat(-1, 1)")));
}
