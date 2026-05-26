// libs/stats/tests/beta_nbin_fit_test.cpp
//
// Regression guard for betafit + nbinfit.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class BetaNbinFitTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── betafit ─────────────────────────────────────────────────────────

TEST_F(BetaNbinFitTest, BetafitRecoversParams)
{
    eval("x = betarnd(2.0, 5.0, 3000, 1); f = betafit(x);");
    EXPECT_NEAR(evalScalar("f(1)"), 2.0, 0.3);
    EXPECT_NEAR(evalScalar("f(2)"), 5.0, 0.5);
}

TEST_F(BetaNbinFitTest, BetafitShapeOneByTwo)
{
    eval("f = betafit(betarnd(0.5, 0.5, 100, 1));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(f, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(f, 2)")), 2);
}

TEST_F(BetaNbinFitTest, BetafitOutOfRangeThrows)
{
    EXPECT_THROW(eval("betafit([0.2, 0.5, 1.5]);"), std::exception);
}

// ── nbinfit ─────────────────────────────────────────────────────────

TEST_F(BetaNbinFitTest, NbinfitRecoversParams)
{
    eval("y = nbinrnd(3.0, 0.4, 3000, 1); f = nbinfit(y);");
    EXPECT_NEAR(evalScalar("f(1)"), 3.0, 0.7);
    EXPECT_NEAR(evalScalar("f(2)"), 0.4, 0.1);
}

TEST_F(BetaNbinFitTest, NbinfitShapeOneByTwo)
{
    eval("f = nbinfit(nbinrnd(5.0, 0.3, 200, 1));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(f, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(f, 2)")), 2);
}

TEST_F(BetaNbinFitTest, NbinfitUnderdispersedThrows)
{
    // All equal counts → var = 0 < mean → not negative-binomial-like.
    EXPECT_THROW(eval("nbinfit([5, 5, 5, 5, 5]);"), std::exception);
}

TEST_F(BetaNbinFitTest, NbinfitNonIntegerThrows)
{
    EXPECT_THROW(eval("nbinfit([1.5, 2.5, 3.0]);"), std::exception);
}
