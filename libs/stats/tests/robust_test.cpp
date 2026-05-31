// libs/stats/tests/robust_test.cpp
//
// Regression guards for robustfit + robustcov.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class RobustTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── robustfit ──────────────────────────────────────────────────────

// MATLAB convention: robustfit(X, y) adds a constant term by default, so X
// holds ONLY the predictor columns and b = [intercept; slopes...]. (numkit
// previously required the caller to supply the constant column and returned
// no intercept — 2026-05-31.)
TEST_F(RobustTest, RobustfitRecoversTrueSlopeWithOutliers)
{
    eval("n = 100; x = (1:n)';"
         "y = 2 * x + 1 + 0.5 * randn(n, 1);"
         "y(95:100) = y(95:100) + 50;"   // 6 outliers
         "b_ols = regress(y, [ones(n,1), x], 0.05);"  // [intercept; slope]
         "[b_r, ~] = robustfit(x, y);"                 // [intercept; slope]
         "err_ols = abs(b_ols(2) - 2);"
         "err_r   = abs(b_r(2)  - 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b_r)")), 2);
    EXPECT_LT(evalScalar("err_r"), 0.1);
    EXPECT_LT(evalScalar("err_r"), evalScalar("err_ols"));
}

TEST_F(RobustTest, RobustfitHuberOption)
{
    eval("n = 80; x = (1:n)';"
         "y = 1.5 * x + 0.5 + 0.5 * randn(n, 1);"
         "y(70:80) = y(70:80) + 100;"
         "[b, ~] = robustfit(x, y, 'huber');");
    EXPECT_NEAR(evalScalar("b(2)"), 1.5, 0.1);   // slope is b(2)
}

TEST_F(RobustTest, RobustfitReturnsScalarScale)
{
    eval("x = (1:50)';"
         "y = 2 * x + randn(50, 1);"
         "[b, s] = robustfit(x, y);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 2);  // intercept + slope
    EXPECT_EQ(static_cast<int>(evalScalar("numel(s)")), 1);
    EXPECT_GT(evalScalar("s"), 0.0);
}

// Exact recovery: with the outlier fully downweighted by the bisquare
// weight, the fit equals the noise-free model. vs MATLAB R2025b.
TEST_F(RobustTest, RobustfitDefaultIntercept)
{
    // y = 2x + 1 with one gross outlier -> b = [1; 2] exactly.
    eval("x = (1:10)'; y = 2*x + 1; y(5) = 100; b = robustfit(x, y);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 2);
    EXPECT_NEAR(evalScalar("b(1)"), 1.0, 1e-7);
    EXPECT_NEAR(evalScalar("b(2)"), 2.0, 1e-7);
    // Two predictors: y = 3 + 2x + 0.5x^2 + outlier -> b = [3; 2; 0.5].
    eval("X2 = [x, x.^2]; y2 = 3 + 2*x + 0.5*x.^2; y2(7) = 200; b2 = robustfit(X2, y2);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b2)")), 3);
    EXPECT_NEAR(evalScalar("b2(1)"), 3.0, 1e-6);
    EXPECT_NEAR(evalScalar("b2(2)"), 2.0, 1e-6);
    EXPECT_NEAR(evalScalar("b2(3)"), 0.5, 1e-6);
    // const='off' suppresses the intercept -> single coefficient.
    eval("bo = robustfit(x, y, 'bisquare', [], 'off');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(bo)")), 1);
}

// ── robustcov ──────────────────────────────────────────────────────

TEST_F(RobustTest, RobustcovRecoversCleanGaussianCov)
{
    eval("n = 500; X = randn(n, 2);"
         "[sig, mu] = robustcov(X);");
    EXPECT_NEAR(evalScalar("sig(1, 1)"), 1.0, 0.3);
    EXPECT_NEAR(evalScalar("sig(2, 2)"), 1.0, 0.3);
    EXPECT_LT(std::abs(evalScalar("sig(1, 2)")), 0.3);
    EXPECT_LT(std::abs(evalScalar("mu(1)")), 0.3);
    EXPECT_LT(std::abs(evalScalar("mu(2)")), 0.3);
}

TEST_F(RobustTest, RobustcovDownweightsOutliers)
{
    eval("n = 200; X = randn(n, 2);"
         "X(180:200, :) = 20 * randn(21, 2);"
         "C_classical = cov(X);"
         "[sig, ~] = robustcov(X);"
         "ratio = sig(1, 1) / C_classical(1, 1);");
    // Robust diag should be << classical when outliers present.
    EXPECT_LT(evalScalar("ratio"), 0.3);
    EXPECT_LT(evalScalar("sig(1, 1)"), 3.0);   // close to 1, far from ~30 classical
}

TEST_F(RobustTest, RobustcovMatchesClassicalOnCleanData)
{
    eval("rng(0); n = 5000; X = randn(n, 2);"   // many samples → both converge
         "C_classical = cov(X);"
         "[sig, ~] = robustcov(X);"
         "err = max(max(abs(sig - C_classical)));");
    // With h = 0.75n and clean data, robust ≈ classical (small bias).
    EXPECT_LT(evalScalar("err"), 0.2);
}

TEST_F(RobustTest, RobustcovTooFewObsThrows)
{
    EXPECT_THROW(eval("robustcov(randn(2, 2));"), std::exception);
}
