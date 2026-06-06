// libs/stats/tests/binoinv_test.cpp
// binoinv.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BinoinvTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(BinoinvTest, Median)
{
    EXPECT_DOUBLE_EQ(evalScalar("binoinv(0.5, 10, 0.3)"), 3.0);
}

TEST_F(BinoinvTest, VectorQ)
{
    eval("x = binoinv([0.05 0.5 0.95], 10, 0.3);");
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(3)"), 5.0);
}

TEST_F(BinoinvTest, BoundaryQuantiles)
{
    EXPECT_DOUBLE_EQ(evalScalar("binoinv(0, 10, 0.3)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("binoinv(1, 10, 0.3)"), 10.0);
}

TEST_F(BinoinvTest, ExtremeProb)
{
    EXPECT_DOUBLE_EQ(evalScalar("binoinv(0.5, 10, 0)"),  0.0);    // p=0 → always 0
    EXPECT_DOUBLE_EQ(evalScalar("binoinv(0.5, 10, 1)"), 10.0);    // p=1 → always n
}

TEST_F(BinoinvTest, InvalidReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("binoinv(-0.1, 10, 0.3)")));
    EXPECT_TRUE(std::isnan(evalScalar("binoinv(1.5,  10, 0.3)")));
    EXPECT_TRUE(std::isnan(evalScalar("binoinv(0.5,  10, -0.1)")));
    EXPECT_TRUE(std::isnan(evalScalar("binoinv(0.5,  -1, 0.3)")));
    EXPECT_TRUE(std::isnan(evalScalar("binoinv(0.5,  2.5, 0.3)")));  // non-integer n
}
