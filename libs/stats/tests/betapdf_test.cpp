// libs/stats/tests/betapdf_test.cpp
// betapdf — coverage gap fix (no behavioral
// change). Reference values from MATLAB R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BetapdfTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(BetapdfTest, ScalarPeak)
{
    // Beta(2,3) peak in (0,1) = (a-1)/(a+b-2) = 1/3, value = 1.5 at x=0.5.
    EXPECT_NEAR(evalScalar("betapdf(0.5, 2, 3)"), 1.5, 1e-12);
}

TEST_F(BetapdfTest, VectorInputs)
{
    eval("y = betapdf([0.1 0.5 0.9]', 2, 3);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.972, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 1.5,   1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 0.108, 1e-12);
}

TEST_F(BetapdfTest, OutOfSupportReturnsZero)
{
    // x outside (0, 1) → 0 (boundaries inclusive on the zero side).
    EXPECT_DOUBLE_EQ(evalScalar("betapdf(-0.1, 2, 3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("betapdf( 0.0, 2, 3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("betapdf( 1.0, 2, 3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("betapdf( 1.5, 2, 3)"), 0.0);
}

TEST_F(BetapdfTest, InvalidShapeReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("betapdf(0.5,  0, 3)")));
    EXPECT_TRUE(std::isnan(evalScalar("betapdf(0.5,  2, 0)")));
    EXPECT_TRUE(std::isnan(evalScalar("betapdf(0.5, -1, 3)")));
    EXPECT_TRUE(std::isnan(evalScalar("betapdf(0.5,  2, -1)")));
}
