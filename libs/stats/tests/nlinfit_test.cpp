// libs/stats/tests/nlinfit_test.cpp
//
// Regression guards for the nonlinear-regression family:
//   nlinfit  — Levenberg-Marquardt NLS
//   nlparci  — parameter confidence intervals
//   nlpredci — prediction confidence intervals

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class NlinfitTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── nlinfit ─────────────────────────────────────────────────────────

// Exponential decay y = a · exp(-b · x). Fit must recover (a, b).
TEST_F(NlinfitTest, RecoversExponentialDecayParameters)
{
    eval("x = linspace(0, 5, 50)';"
         "beta_true = [2.0; 0.5];"
         "y = beta_true(1) * exp(-beta_true(2) * x) + 0.02 * randn(size(x));"
         "fun = @(b, x) b(1) * exp(-b(2) * x);"
         "[beta, R, J] = nlinfit(x, y, fun, [1.0; 1.0]);"
         "err = max(abs(beta - beta_true));");
    EXPECT_LT(evalScalar("err"), 0.05);
    EXPECT_EQ(static_cast<int>(evalScalar("size(beta, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(R)")), 50);
    EXPECT_EQ(static_cast<int>(evalScalar("size(J, 1)")), 50);
    EXPECT_EQ(static_cast<int>(evalScalar("size(J, 2)")), 2);
}

// Quadratic y = a·x² + b·x + c.
TEST_F(NlinfitTest, RecoversQuadraticCoefficients)
{
    eval("x = linspace(-3, 3, 40)';"
         "y = 1.5 * x.^2 - 0.5 * x + 1.0;"
         "fun = @(b, x) b(1) * x.^2 + b(2) * x + b(3);"
         "[beta, ~, ~] = nlinfit(x, y, fun, [1.0; 1.0; 1.0]);");
    EXPECT_NEAR(evalScalar("beta(1)"),  1.5, 1e-6);
    EXPECT_NEAR(evalScalar("beta(2)"), -0.5, 1e-6);
    EXPECT_NEAR(evalScalar("beta(3)"),  1.0, 1e-6);
}

// MSE = ||R||² / (n - p).
TEST_F(NlinfitTest, MSEDefinitionMatches)
{
    eval("x = (1:30)';"
         "y = 2.0 * x + 1.0;"   // exact linear → SSE = 0
         "fun = @(b, x) b(1) * x + b(2);"
         "[beta, R, ~, ~, MSE] = nlinfit(x, y, fun, [0.5; 0.5]);"
         "ssemse_ratio = sum(R.^2) / (30 - 2);"
         "err = abs(MSE - ssemse_ratio);");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

// ── nlparci ─────────────────────────────────────────────────────────

TEST_F(NlinfitTest, ParciContainsTrueBetaUnderNoise)
{
    eval("x = linspace(0, 1, 200)';"
         "beta_true = [1.5; 2.0];"
         "y = beta_true(1) * x + beta_true(2) + 0.05 * randn(size(x));"
         "fun = @(b, x) b(1) * x + b(2);"
         "[beta, R, J] = nlinfit(x, y, fun, [1.0; 1.0]);"
         "ci = nlparci(beta, R, J);"
         "in1 = (ci(1, 1) <= beta_true(1)) && (beta_true(1) <= ci(1, 2));"
         "in2 = (ci(2, 1) <= beta_true(2)) && (beta_true(2) <= ci(2, 2));");
    EXPECT_TRUE(evalScalar("in1") > 0.5);
    EXPECT_TRUE(evalScalar("in2") > 0.5);
}

TEST_F(NlinfitTest, ParciAlphaControlsWidth)
{
    eval("x = linspace(0, 1, 100)';"
         "y = 2 * x + 0.05 * randn(size(x));"
         "fun = @(b, x) b(1) * x + b(2);"
         "[beta, R, J] = nlinfit(x, y, fun, [1; 0]);"
         "ci99 = nlparci(beta, R, J, 0.01);"
         "ci90 = nlparci(beta, R, J, 0.10);"
         "w99 = ci99(1, 2) - ci99(1, 1);"
         "w90 = ci90(1, 2) - ci90(1, 1);");
    EXPECT_GT(evalScalar("w99"), evalScalar("w90"));
}

// ── nlpredci ────────────────────────────────────────────────────────

TEST_F(NlinfitTest, PredciReturnsYpredAndDelta)
{
    eval("x = linspace(0, 1, 100)';"
         "y = 2 * x + 1 + 0.05 * randn(size(x));"
         "fun = @(b, x) b(1) * x + b(2);"
         "[beta, R, J] = nlinfit(x, y, fun, [1; 0]);"
         "[yp, d] = nlpredci(fun, x(1:5), beta, R, J, 0.05);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(yp)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(d)")), 5);
    EXPECT_TRUE(evalScalar("all(d > 0)") > 0.5);
}

TEST_F(NlinfitTest, PredciDeltaScalesWithUncertainty)
{
    eval("x = linspace(0, 1, 100)';"
         "y_lo = 2 * x + 0.01 * randn(size(x));"
         "y_hi = 2 * x + 0.30 * randn(size(x));"
         "fun = @(b, x) b(1) * x + b(2);"
         "[b1, R1, J1] = nlinfit(x, y_lo, fun, [1; 0]);"
         "[b2, R2, J2] = nlinfit(x, y_hi, fun, [1; 0]);"
         "[~, d1] = nlpredci(fun, x(1:5), b1, R1, J1, 0.05);"
         "[~, d2] = nlpredci(fun, x(1:5), b2, R2, J2, 0.05);");
    EXPECT_GT(evalScalar("mean(d2)"), evalScalar("mean(d1)"));
}
