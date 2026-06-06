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
    StdEngine engine;
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

// ── CI: gamfit + wblfit (Wald, observed Fisher info) ────────────────

TEST_F(DistFitsTest, GamfitCIMatchesMatlab)
{
    // Deterministic Gamma(3, 2) sample. MATLAB CI:
    //   [2.8302 1.8746; 3.1838 2.1309]
    eval(R"(
        n=2000; u=((1:n)' - 0.5)/n; x = gaminv(u, 3.0, 2.0);
        [p, pci] = gamfit(x);
    )");
    EXPECT_NEAR(evalScalar("pci(1, 1)"), 2.8302, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 1)"), 3.1838, 1e-3);
    EXPECT_NEAR(evalScalar("pci(1, 2)"), 1.8746, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 2)"), 2.1309, 1e-3);
}

TEST_F(DistFitsTest, WblfitCIMatchesMatlab)
{
    // Deterministic Wbl(2, 1.5) sample. MATLAB CI:
    //   [1.9394 1.4502; 2.0625 1.5528]
    eval(R"(
        n=2000; u=((1:n)' - 0.5)/n; x = wblinv(u, 2.0, 1.5);
        [p, pci] = wblfit(x);
    )");
    EXPECT_NEAR(evalScalar("pci(1, 1)"), 1.9394, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 1)"), 2.0625, 1e-3);
    EXPECT_NEAR(evalScalar("pci(1, 2)"), 1.4502, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 2)"), 1.5528, 1e-3);
}

TEST_F(DistFitsTest, GamfitCIShape)
{
    eval("[p, pci] = gamfit(gaminv(((1:200)' - 0.5)/200, 3.0, 2.0));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(pci, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(pci, 2)")), 2);
}

TEST_F(DistFitsTest, GamfitAlphaArgument)
{
    eval(R"(
        x = gaminv(((1:500)' - 0.5)/500, 3.0, 2.0);
        [~, ci95] = gamfit(x);
        [~, ci99] = gamfit(x, 0.01);
    )");
    // Wider CI at α=0.01.
    EXPECT_GT(evalScalar("ci99(2, 1) - ci99(1, 1)"),
              evalScalar("ci95(2, 1) - ci95(1, 1)"));
}

// ── wblfit censoring + freq ─────────────────────────────────────────

TEST_F(DistFitsTest, WblfitCensoredMatchesMatlab)
{
    // Right-censor top 20% of Wbl(2, 1.5) sample, n=1500.
    // MATLAB → parm=(2.000016, 1.500328); CI bit-exact.
    eval(R"(
        n=1500; u=((1:n)' - 0.5)/n; x = wblinv(u, 2.0, 1.5);
        thr = quantile(x, 0.8);
        xc = min(x, thr); cens = (x >= thr);
        [p, pci] = wblfit(xc, 0.05, cens);
    )");
    EXPECT_NEAR(evalScalar("p(1)"), 2.000016, 1e-4);
    EXPECT_NEAR(evalScalar("p(2)"), 1.500328, 1e-4);
    EXPECT_NEAR(evalScalar("pci(1, 1)"), 1.9259, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 1)"), 2.0770, 1e-3);
    EXPECT_NEAR(evalScalar("pci(1, 2)"), 1.4289, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 2)"), 1.5753, 1e-3);
}

TEST_F(DistFitsTest, WblfitFreqMatchesExplicitRep)
{
    // freq=[3 2 1 2 3] on [1..5] ≡ expanded [1,1,1,2,2,3,4,4,5,5,5].
    // MATLAB → parm=(3.393773, 1.987882).
    eval(R"(
        xv = [1 2 3 4 5]; fv = [3 2 1 2 3];
        pa = wblfit(xv, 0.05, [], fv);
        xexp = [1 1 1 2 2 3 4 4 5 5 5];
        pb = wblfit(xexp);
    )");
    EXPECT_NEAR(evalScalar("pa(1)"), 3.393773, 1e-4);
    EXPECT_NEAR(evalScalar("pa(2)"), 1.987882, 1e-4);
    // freq form == explicit replication.
    EXPECT_NEAR(evalScalar("pa(1)"), evalScalar("pb(1)"), 1e-6);
    EXPECT_NEAR(evalScalar("pa(2)"), evalScalar("pb(2)"), 1e-6);
}

TEST_F(DistFitsTest, WblfitCensLengthMismatchThrows)
{
    EXPECT_THROW(eval("wblfit([1 2 3], 0.05, [0 0]);"), std::exception);
}

// ── gamfit censoring + freq ─────────────────────────────────────────

TEST_F(DistFitsTest, GamfitCensoredMatchesMatlab)
{
    // Right-censor top 20% of Gamma(3, 2) sample, n=1500.
    // MATLAB MLE → (3.0015906199, 1.9988661241).
    eval(R"(
        n=1500; u=((1:n)' - 0.5)/n; x = gaminv(u, 3.0, 2.0);
        thr = quantile(x, 0.8);
        xc = min(x, thr); cens = (x >= thr);
        [p, pci] = gamfit(xc, 0.05, cens);
    )");
    EXPECT_NEAR(evalScalar("p(1)"), 3.0015906199, 1e-5);
    EXPECT_NEAR(evalScalar("p(2)"), 1.9988661241, 1e-5);
    EXPECT_NEAR(evalScalar("pci(1, 1)"), 2.7791, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 1)"), 3.2419, 1e-3);
    EXPECT_NEAR(evalScalar("pci(1, 2)"), 1.8297, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 2)"), 2.1836, 1e-3);
}

TEST_F(DistFitsTest, GamfitFreqMatchesExplicitRep)
{
    eval(R"(
        xv = [1 2 3 4 5]; fv = [3 2 1 2 3];
        pa = gamfit(xv, 0.05, [], fv);
        xexp = [1 1 1 2 2 3 4 4 5 5 5];
        pb = gamfit(xexp);
    )");
    EXPECT_NEAR(evalScalar("pa(1)"), 2.9074324264, 1e-5);
    EXPECT_NEAR(evalScalar("pa(2)"), 1.0318382545, 1e-5);
    EXPECT_NEAR(evalScalar("pa(1)"), evalScalar("pb(1)"), 1e-6);
    EXPECT_NEAR(evalScalar("pa(2)"), evalScalar("pb(2)"), 1e-6);
}

TEST_F(DistFitsTest, GamfitCensLengthMismatchThrows)
{
    EXPECT_THROW(eval("gamfit([1 2 3], 0.05, [0 0]);"), std::exception);
}
