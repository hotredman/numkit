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

TEST_F(RobustTest, RobustfitRecoversTrueSlopeWithOutliers)
{
    eval("n = 100; x = (1:n)'; X = [x, ones(n, 1)];"
         "y = 2 * x + 1 + 0.5 * randn(n, 1);"
         "y(95:100) = y(95:100) + 50;"   // 6 outliers
         "b_ols = regress(y, X, 0.05);"
         "[b_r, ~] = robustfit(X, y);"
         "err_ols = abs(b_ols(1) - 2);"
         "err_r   = abs(b_r(1)  - 2);");
    EXPECT_LT(evalScalar("err_r"), 0.1);
    EXPECT_LT(evalScalar("err_r"), evalScalar("err_ols"));
}

TEST_F(RobustTest, RobustfitHuberOption)
{
    eval("n = 80; x = (1:n)'; X = [x, ones(n, 1)];"
         "y = 1.5 * x + 0.5 + 0.5 * randn(n, 1);"
         "y(70:80) = y(70:80) + 100;"
         "[b, ~] = robustfit(X, y, 'huber');");
    EXPECT_NEAR(evalScalar("b(1)"), 1.5, 0.1);
}

TEST_F(RobustTest, RobustfitReturnsScalarScale)
{
    eval("X = (1:50)'; X = [X, ones(50, 1)];"
         "y = 2 * X(:, 1) + randn(50, 1);"
         "[b, s] = robustfit(X, y);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(s)")), 1);
    EXPECT_GT(evalScalar("s"), 0.0);
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
