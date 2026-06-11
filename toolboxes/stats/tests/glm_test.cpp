// toolboxes/stats/tests/glm_test.cpp
//
// Regression guards for the GLM family: glmfit + glmval.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class GlmTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── glmfit — logistic regression ────────────────────────────────────

TEST_F(GlmTest, LogisticRecoversCoefficients)
{
    eval("n = 500; x = randn(n, 1) * 2;"
         "eta = 0.5 + 1.5 * x; p = 1 ./ (1 + exp(-eta));"
         "y = double(rand(n, 1) < p);"
         "[b, dev] = glmfit(x, y, 'binomial');");
    EXPECT_NEAR(evalScalar("b(1)"), 0.5, 0.3);   // intercept
    EXPECT_NEAR(evalScalar("b(2)"), 1.5, 0.3);   // slope
    EXPECT_GT(evalScalar("dev"), 0.0);
}

TEST_F(GlmTest, LogisticPredictionsAreProbabilities)
{
    eval("n = 200; x = randn(n, 1);"
         "y = double(rand(n, 1) < 1 ./ (1 + exp(-x)));"
         "[b, ~] = glmfit(x, y, 'binomial');"
         "xq = (-3:0.5:3)'; yhat = glmval(b, xq, 'logit');"
         "in01 = all(yhat >= 0 & yhat <= 1);");
    EXPECT_TRUE(evalScalar("in01") > 0.5);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(yhat)")), 13);
}

// ── glmfit — Poisson regression ─────────────────────────────────────

TEST_F(GlmTest, PoissonLogLinkFit)
{
    // Use a deterministic exact-Poisson-mean response: y = exp(0.7 + 0.5*x)
    // rounded to integers. Not strictly Poisson-distributed but the log
    // link recovers the linear predictor exactly when the means align.
    eval("x = linspace(-1, 1, 200)';"
         "lambda = exp(0.7 + 0.5 * x);"
         "y = round(lambda);"
         "[b, ~] = glmfit(x, y, 'poisson');");
    EXPECT_NEAR(evalScalar("b(1)"), 0.7, 0.05);
    EXPECT_NEAR(evalScalar("b(2)"), 0.5, 0.05);
}

// ── glmfit — Normal (≡ OLS) ─────────────────────────────────────────

TEST_F(GlmTest, NormalIdentityMatchesOLS)
{
    eval("n = 200; x = randn(n, 1);"
         "y = 2.0 + 1.5 * x + 0.3 * randn(n, 1);"
         "[b_glm, ~] = glmfit(x, y, 'normal');"
         "X = [ones(n, 1), x];"
         "b_ols = regress(y, X, 0.05);"
         "err = max(abs(b_glm - b_ols));");
    EXPECT_LT(evalScalar("err"), 1e-6);
}

// ── glmval ──────────────────────────────────────────────────────────

TEST_F(GlmTest, GlmvalShapesPMinusOneOK)
{
    eval("X = randn(50, 3);"
         "b = [1.0; 0.5; -0.2; 0.7];"   // intercept + 3 slopes
         "yhat = glmval(b, X, 'identity');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(yhat)")), 50);
}

TEST_F(GlmTest, GlmvalIdentityIsLinear)
{
    eval("X = (0:9)';"
         "b = [1.0; 2.0];"   // intercept=1, slope=2
         "yhat = glmval(b, X, 'identity');"
         "ref = 1 + 2 * X;"
         "err = max(abs(yhat - ref));");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

TEST_F(GlmTest, GlmvalLogIsExpEta)
{
    eval("X = (0:5)';"
         "b = [0.5; 0.2];"
         "yhat = glmval(b, X, 'log');"
         "ref = exp(0.5 + 0.2 * X);"
         "err = max(abs(yhat - ref));");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

// ── Error handling ──────────────────────────────────────────────────

TEST_F(GlmTest, GlmfitUnknownDistrThrows)
{
    EXPECT_THROW(eval("glmfit(randn(10, 1), randn(10, 1), 'wat');"),
                 std::exception);
}

TEST_F(GlmTest, GlmvalShapeMismatchThrows)
{
    EXPECT_THROW(eval("glmval([1; 2], randn(10, 5), 'identity');"),
                 std::exception);
}
