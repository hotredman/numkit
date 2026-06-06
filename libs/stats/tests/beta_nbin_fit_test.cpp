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
    StdEngine engine;
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

// ── CI (Wald, observed Fisher information, MATLAB transforms) ───────

TEST_F(BetaNbinFitTest, BetafitCIMatchesMatlab)
{
    // Deterministic Beta(2, 5) sample. MATLAB CI:
    //   [1.8883 4.6999; 2.1207 5.3258]
    // numkit uses analytical Hessian (trigamma); MATLAB betafit uses
    // a simulation-based CI, so we match to ~5e-4 in width.
    eval(R"(
        n=2000; u=((1:n)' - 0.5)/n; x = betainv(u, 2.0, 5.0);
        [p, pci] = betafit(x);
    )");
    EXPECT_NEAR(evalScalar("pci(1, 1)"), 1.8883, 0.005);
    EXPECT_NEAR(evalScalar("pci(2, 1)"), 2.1207, 0.005);
    EXPECT_NEAR(evalScalar("pci(1, 2)"), 4.6999, 0.005);
    EXPECT_NEAR(evalScalar("pci(2, 2)"), 5.3258, 0.005);
}

TEST_F(BetaNbinFitTest, NbinfitCIMatchesMatlab)
{
    // Deterministic NB(3, 0.4) sample via nbininv. MATLAB CI:
    //   [2.6761 0.3729; 3.3270 0.4272]
    eval(R"(
        n=2000; u=((1:n)' - 0.5)/n; x = round(nbininv(u, 3.0, 0.4));
        [p, pci] = nbinfit(x);
    )");
    EXPECT_NEAR(evalScalar("pci(1, 1)"), 2.6761, 0.025);
    EXPECT_NEAR(evalScalar("pci(2, 1)"), 3.3270, 0.025);
    EXPECT_NEAR(evalScalar("pci(1, 2)"), 0.3729, 0.005);
    EXPECT_NEAR(evalScalar("pci(2, 2)"), 0.4272, 0.005);
}

TEST_F(BetaNbinFitTest, BetafitCIShape)
{
    eval("[p, pci] = betafit(betainv(((1:200)' - 0.5)/200, 2.0, 5.0));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(pci, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(pci, 2)")), 2);
    // Lower row < parmhat < upper row.
    EXPECT_LT(evalScalar("pci(1, 1)"), evalScalar("p(1)"));
    EXPECT_LT(evalScalar("p(1)"),      evalScalar("pci(2, 1)"));
}

TEST_F(BetaNbinFitTest, BetafitAlphaArgument)
{
    // Narrower CI at α=0.01 than at default α=0.05.
    eval(R"(
        x = betainv(((1:500)' - 0.5)/500, 2.0, 5.0);
        [~, ci95] = betafit(x);
        [~, ci99] = betafit(x, 0.01);
    )");
    EXPECT_GT(evalScalar("ci99(2, 1) - ci99(1, 1)"),
              evalScalar("ci95(2, 1) - ci95(1, 1)"));
}
