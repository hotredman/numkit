// libs/stats/tests/dist_fits_test.cpp
//
// Regression guard for gamfit + wblfit.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class DistFitsTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── gamfit ──────────────────────────────────────────────────────────

TEST_F(DistFitsTest, GamfitRecoversTrueParams)
{
    eval("x = gamrnd(2.0, 3.0, 2000, 1); fit = gamfit(x);");
    EXPECT_NEAR(evalScalar("fit(1)"), 2.0, 0.3);   // shape
    EXPECT_NEAR(evalScalar("fit(2)"), 3.0, 0.4);   // scale
}

TEST_F(DistFitsTest, GamfitShapeOneByTwo)
{
    eval("fit = gamfit(gamrnd(1.5, 1.0, 100, 1));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(fit, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(fit, 2)")), 2);
}

TEST_F(DistFitsTest, GamfitNegativeDataThrows)
{
    EXPECT_THROW(eval("gamfit([1, 2, -3, 4]);"), std::exception);
}

// Identical observations → infinite shape (degenerate).
TEST_F(DistFitsTest, GamfitConstantDataReturnsInfShape)
{
    eval("fit = gamfit([3, 3, 3, 3, 3]); is_inf = isinf(fit(1));");
    EXPECT_TRUE(evalScalar("is_inf") > 0.5);
}

// ── wblfit ──────────────────────────────────────────────────────────

TEST_F(DistFitsTest, WblfitRecoversTrueParams)
{
    eval("y = wblrnd(3.0, 2.0, 2000, 1); fit = wblfit(y);");
    EXPECT_NEAR(evalScalar("fit(1)"), 3.0, 0.3);   // scale
    EXPECT_NEAR(evalScalar("fit(2)"), 2.0, 0.3);   // shape
}

TEST_F(DistFitsTest, WblfitShapeOneByTwo)
{
    eval("fit = wblfit(wblrnd(2.0, 1.5, 100, 1));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(fit, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(fit, 2)")), 2);
}

TEST_F(DistFitsTest, WblfitNegativeDataThrows)
{
    EXPECT_THROW(eval("wblfit([1, 2, -3, 4]);"), std::exception);
}
