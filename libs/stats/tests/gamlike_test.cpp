// libs/stats/tests/gamlike_test.cpp
// Backfill gtest + gamlike. Reference values
// from MATLAB R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GamlikeTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1 2 3 4 5]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(GamlikeTest, BasicNLogL)
{
    EXPECT_NEAR(evalScalar("gamlike([2, 1], x)"), 10.2125082572, 1e-9);
}

TEST_F(GamlikeTest, AVarBasic)
{
    eval("[nL, av] = gamlike([2, 1], x);");
    EXPECT_NEAR(evalScalar("av(1,1)"),  0.5064136442, 1e-7);
    EXPECT_NEAR(evalScalar("av(1,2)"), -0.1266034110, 1e-7);
    EXPECT_NEAR(evalScalar("av(2,1)"), -0.1266034110, 1e-7);  // symmetry
    EXPECT_NEAR(evalScalar("av(2,2)"),  0.0816508528, 1e-7);
}

TEST_F(GamlikeTest, InvalidShapeReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("gamlike([0, 1], x)")));
    EXPECT_TRUE(std::isnan(evalScalar("gamlike([-1, 1], x)")));
}

TEST_F(GamlikeTest, InvalidScaleReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("gamlike([2, 0], x)")));
    EXPECT_TRUE(std::isnan(evalScalar("gamlike([2, -1], x)")));
}

TEST_F(GamlikeTest, NonPositiveDataReturnsInf)
{
    // x <= 0 is outside Gamma's support → log f = -inf → nL = +inf.
    EXPECT_TRUE(std::isinf(evalScalar("gamlike([2, 1], [1 2 0 4]')")));
}

TEST_F(GamlikeTest, EmptyDataReturnsInf)
{
    EXPECT_TRUE(std::isinf(evalScalar("gamlike([2, 1], [])")));
}
