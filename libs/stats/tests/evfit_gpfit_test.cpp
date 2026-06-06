// libs/stats/tests/evfit_gpfit_test.cpp
//
// Regression guard for evfit + gpfit.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class EvfitGpfitTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── evfit ───────────────────────────────────────────────────────────

TEST_F(EvfitGpfitTest, EvfitRecoversParams)
{
    eval("x = evrnd(1.0, 2.0, 3000, 1); f = evfit(x);");
    EXPECT_NEAR(evalScalar("f(1)"), 1.0, 0.25);
    EXPECT_NEAR(evalScalar("f(2)"), 2.0, 0.3);
}

TEST_F(EvfitGpfitTest, EvfitShapeOneByTwo)
{
    eval("f = evfit(evrnd(0.0, 1.0, 100, 1));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(f, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(f, 2)")), 2);
}

TEST_F(EvfitGpfitTest, EvfitZeroVarianceThrows)
{
    EXPECT_THROW(eval("evfit([3.0, 3.0, 3.0, 3.0, 3.0]);"), std::exception);
}

TEST_F(EvfitGpfitTest, EvfitTooFewObsThrows)
{
    EXPECT_THROW(eval("evfit([1.0]);"), std::exception);
}

// ── gpfit ───────────────────────────────────────────────────────────

TEST_F(EvfitGpfitTest, GpfitRecoversParams)
{
    eval("y = gprnd(0.3, 1.5, 0, 3000, 1); f = gpfit(y);");
    EXPECT_NEAR(evalScalar("f(1)"), 0.3, 0.2);
    EXPECT_NEAR(evalScalar("f(2)"), 1.5, 0.4);
}

TEST_F(EvfitGpfitTest, GpfitShapeOneByTwo)
{
    eval("f = gpfit(gprnd(0.0, 1.0, 0, 100, 1));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(f, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(f, 2)")), 2);
}

TEST_F(EvfitGpfitTest, GpfitExponentialLimit)
{
    // k = 0 → exponential with mean σ; expect k̂ near 0.
    eval("y = exprnd(1.0, 5000, 1); f = gpfit(y);");
    EXPECT_NEAR(evalScalar("f(1)"), 0.0, 0.2);
    EXPECT_NEAR(evalScalar("f(2)"), 1.0, 0.2);
}

TEST_F(EvfitGpfitTest, GpfitNegativeXThrows)
{
    EXPECT_THROW(eval("gpfit([0.5, 1.0, -0.3, 2.0]);"), std::exception);
}

// ── Grimshaw MLE — bit-equal MATLAB references on deterministic data ─

TEST_F(EvfitGpfitTest, GpfitGrimshawMatchesMatlabPositiveK)
{
    // Deterministic GP(0.3, 1.5) inverse-CDF sample, n=2000.
    // MATLAB MLE → k=0.2991052958, σ=1.5010041220.
    eval("n=2000; u=((1:n)' - 0.5)/n; x = gpinv(u, 0.3, 1.5, 0); f = gpfit(x);");
    EXPECT_NEAR(evalScalar("f(1)"), 0.2991052958, 1e-5);
    EXPECT_NEAR(evalScalar("f(2)"), 1.5010041220, 1e-5);
}

TEST_F(EvfitGpfitTest, GpfitGrimshawMatchesMatlabNegativeK)
{
    // Deterministic GP(-0.1, 2.0), n=1500.
    // MATLAB MLE → k=-0.1021089595, σ=2.0038001270.
    eval("n=1500; u=((1:n)' - 0.5)/n; x = gpinv(u, -0.1, 2.0, 0); f = gpfit(x);");
    EXPECT_NEAR(evalScalar("f(1)"), -0.1021089595, 1e-5);
    EXPECT_NEAR(evalScalar("f(2)"),  2.0038001270, 1e-5);
}

TEST_F(EvfitGpfitTest, GpfitGrimshawMatchesMatlabExponentialLimit)
{
    // Deterministic GP(0, 1.5) (exponential), n=1000.
    // MATLAB MLE → k=-0.0025358440, σ=1.5032822883.
    eval("n=1000; u=((1:n)' - 0.5)/n; x = gpinv(u, 0, 1.5, 0); f = gpfit(x);");
    EXPECT_NEAR(evalScalar("f(1)"), -0.0025358440, 1e-5);
    EXPECT_NEAR(evalScalar("f(2)"),  1.5032822883, 1e-5);
}

// ── CI (Wald, observed Fisher info, MATLAB transforms) ──────────────

TEST_F(EvfitGpfitTest, EvfitCIMatchesMatlab)
{
    // Deterministic EV(1, 2) sample. MATLAB CI:
    //   [0.9077 1.9320; 1.0922 2.0687]
    eval(R"(
        n=2000; u=((1:n)' - 0.5)/n; x = evinv(u, 1.0, 2.0);
        [p, pci] = evfit(x);
    )");
    EXPECT_NEAR(evalScalar("pci(1, 1)"), 0.9077, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 1)"), 1.0922, 1e-3);
    EXPECT_NEAR(evalScalar("pci(1, 2)"), 1.9320, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 2)"), 2.0687, 1e-3);
}

TEST_F(EvfitGpfitTest, GpfitCIMatchesMatlab)
{
    // Deterministic GP(0.3, 1.5) sample. MATLAB CI:
    //   [0.2421 1.3985; 0.3561 1.6110]
    eval(R"(
        n=2000; u=((1:n)' - 0.5)/n; x = gpinv(u, 0.3, 1.5, 0);
        [p, pci] = gpfit(x);
    )");
    EXPECT_NEAR(evalScalar("pci(1, 1)"), 0.2421, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 1)"), 0.3561, 1e-3);
    EXPECT_NEAR(evalScalar("pci(1, 2)"), 1.3985, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 2)"), 1.6110, 1e-3);
}

TEST_F(EvfitGpfitTest, EvfitCIShape)
{
    eval("[p, pci] = evfit(evinv(((1:200)' - 0.5)/200, 0.0, 1.0));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(pci, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(pci, 2)")), 2);
}

// ── evfit censoring + frequency weights ─────────────────────────────

TEST_F(EvfitGpfitTest, EvfitCensoredMatchesMatlab)
{
    // Right-censor top 20%; MATLAB MLE → (1.000018, 1.999662),
    // CI [0.9019 1.9170; 1.0981 2.0859].
    eval(R"(
        n=2000; u=((1:n)' - 0.5)/n; x = evinv(u, 1.0, 2.0);
        thr = quantile(x, 0.8);
        xc = min(x, thr);  cens = (x >= thr);
        [p, pci] = evfit(xc, 0.05, cens);
    )");
    EXPECT_NEAR(evalScalar("p(1)"),     1.000018, 1e-4);
    EXPECT_NEAR(evalScalar("p(2)"),     1.999662, 1e-4);
    EXPECT_NEAR(evalScalar("pci(1, 1)"), 0.9019, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 1)"), 1.0981, 1e-3);
    EXPECT_NEAR(evalScalar("pci(1, 2)"), 1.9170, 1e-3);
    EXPECT_NEAR(evalScalar("pci(2, 2)"), 2.0859, 1e-3);
}

TEST_F(EvfitGpfitTest, EvfitFreqMatchesExplicitReplication)
{
    // freq=[3 2 1 2 3] on [1..5] ≡ expanded [1 1 1 2 2 3 4 4 5 5 5].
    // MATLAB → (3.791736, 1.389443).
    eval(R"(
        x_rep = [1 2 3 4 5];
        f_rep = [3 2 1 2 3];
        [pa, ~] = evfit(x_rep, 0.05, [], f_rep);
        x_exp = [1 1 1 2 2 3 4 4 5 5 5];
        pb = evfit(x_exp);
    )");
    EXPECT_NEAR(evalScalar("pa(1)"), 3.791736, 1e-4);
    EXPECT_NEAR(evalScalar("pa(2)"), 1.389443, 1e-4);
    // freq form should match explicit replication.
    EXPECT_NEAR(evalScalar("pa(1)"), evalScalar("pb(1)"), 1e-8);
    EXPECT_NEAR(evalScalar("pa(2)"), evalScalar("pb(2)"), 1e-8);
}

TEST_F(EvfitGpfitTest, EvfitCensoringLengthMismatchThrows)
{
    EXPECT_THROW(eval("evfit([1 2 3], 0.05, [0 0]);"), std::exception);
}

TEST_F(EvfitGpfitTest, EvfitFreqLengthMismatchThrows)
{
    EXPECT_THROW(eval("evfit([1 2 3], 0.05, [], [1 1]);"), std::exception);
}
