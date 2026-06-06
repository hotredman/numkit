// libs/stats/tests/kde_test.cpp
//
// Regression guard for kde — MATLAB R2023b+ alias for ksdensity.
// v1 is a direct adapter alias; this test confirms the alias resolves
// and that the underlying ksdensity machinery still works through it.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class KdeTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(KdeTest, DefaultReturnsHundredPoints)
{
    eval("rng(0); x = randn(50, 1); [f, xi] = kde(x);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(f)")),  100);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(xi)")), 100);
    EXPECT_TRUE(evalScalar("all(isfinite(f))") > 0.5);
}

TEST_F(KdeTest, ExplicitEvaluationPoints)
{
    eval("rng(0); x = randn(50, 1); pts = linspace(-3, 3, 21);"
         "fe = kde(x, pts);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(fe)")), 21);
    // PDF values must be non-negative.
    EXPECT_TRUE(evalScalar("all(fe >= 0)") > 0.5);
}

TEST_F(KdeTest, ThreeOutputsIncludeBandwidth)
{
    eval("rng(0); x = randn(50, 1); [f, xi, bw] = kde(x);");
    EXPECT_GT(evalScalar("bw"), 0.0);
    EXPECT_LT(evalScalar("bw"), 10.0);
}

// Integral over a wide range should approach 1 (PDF property).
TEST_F(KdeTest, IntegratesNearOneOverWideRange)
{
    eval("rng(0); x = randn(500, 1); pts = linspace(-10, 10, 401);"
         "f = kde(x, pts); area = sum(f) * (pts(2) - pts(1));");
    EXPECT_NEAR(evalScalar("area"), 1.0, 0.05);
}

// kde and ksdensity should produce identical output (direct alias).
TEST_F(KdeTest, MatchesKsdensityDirectly)
{
    eval("rng(0); x = randn(50, 1); pts = linspace(-3, 3, 21);"
         "fk = kde(x, pts); fs = ksdensity(x, pts);"
         "err = max(abs(fk - fs));");
    EXPECT_LT(evalScalar("err"), 1e-15);
}
