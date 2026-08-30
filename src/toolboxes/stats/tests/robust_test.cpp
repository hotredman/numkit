// toolboxes/stats/tests/robust_test.cpp
//
// Regression guards for robustfit + robustcov.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class RobustTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("rng(0);"); }
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

// DEEP-PROBE c174: all 9 MATLAB weight functions + the DuMouchel-O'Brien
// leverage adjustment. Deterministic 10-point line with one outlier; values
// pinned to MATLAB R2025b (bit-identical to 8 decimals). numkit previously
// shipped only bisquare/huber AND omitted leverage, so values were off.
TEST_F(RobustTest, RobustfitWeightFunctions)
{
    eval("xw = (1:10)'; yw = [1.1 1.9 3.2 3.9 5.1 6.0 12.0 8.1 9.0 9.9]';");
    eval("ba = robustfit(xw, yw, 'andrews');");
    EXPECT_NEAR(evalScalar("ba(1)"), 0.06889811, 1e-6);
    EXPECT_NEAR(evalScalar("ba(2)"), 0.99137723, 1e-6);
    eval("bc = robustfit(xw, yw, 'cauchy');");
    EXPECT_NEAR(evalScalar("bc(2)"), 0.99170253, 1e-6);
    eval("bf = robustfit(xw, yw, 'fair');");
    EXPECT_NEAR(evalScalar("bf(2)"), 0.99884759, 1e-6);
    eval("bh = robustfit(xw, yw, 'huber');");
    EXPECT_NEAR(evalScalar("bh(2)"), 0.99766569, 1e-6);
    eval("bl = robustfit(xw, yw, 'logistic');");
    EXPECT_NEAR(evalScalar("bl(2)"), 0.99707529, 1e-6);
    eval("bt = robustfit(xw, yw, 'talwar');");
    EXPECT_NEAR(evalScalar("bt(2)"), 0.99166667, 1e-6);
    eval("bwe = robustfit(xw, yw, 'welsch');");
    EXPECT_NEAR(evalScalar("bwe(2)"), 0.99131813, 1e-6);
    eval("bsq = robustfit(xw, yw, 'bisquare');");
    EXPECT_NEAR(evalScalar("bsq(2)"), 0.99138205, 1e-6);
    // 'ols' = ordinary least squares (no downweighting): slope is pulled by
    // the outlier (MATLAB 1.08242424).
    eval("bols = robustfit(xw, yw, 'ols');");
    EXPECT_NEAR(evalScalar("bols(2)"), 1.08242424, 1e-6);
    // An unknown weight function still errors.
    EXPECT_THROW(eval("robustfit(xw, yw, 'nosuchweight');"), std::exception);
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

// regress 4th output rint (residual confidence intervals for outlier
// diagnosis). MATLAB R2025b uses the Chatterjee & Hadi leave-one-out
// variance estimate with (nu-1) dof, NOT the textbook r +/- t*sigma*sqrt(1-h).
// numkit previously returned the textbook form (rint disagreed with MATLAB);
// now bit-exact. Pinned to MATLAB.
TEST_F(RobustTest, RegressResidualIntervals)
{
    eval("y = [1.1 1.9 3.2 3.8 5.1]'; X = [ones(5,1) (1:5)'];");
    eval("[b, bint, r, rint, stats] = regress(y, X);");
    EXPECT_DOUBLE_EQ(evalScalar("size(rint,1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(rint,2)"), 2.0);
    // Each residual lies inside its interval.
    EXPECT_DOUBLE_EQ(evalScalar("double(all(rint(:,1) <= r & r <= rint(:,2)))"), 1.0);
    // Exact MATLAB values (leave-one-out studentized residual interval).
    EXPECT_NEAR(evalScalar("rint(1,1)"), -0.5423713822, 1e-9);
    EXPECT_NEAR(evalScalar("rint(1,2)"),  0.6623713822, 1e-9);
    EXPECT_NEAR(evalScalar("rint(3,1)"), -0.5217414236, 1e-9);
    EXPECT_NEAR(evalScalar("rint(3,2)"),  0.8817414236, 1e-9);
    EXPECT_NEAR(evalScalar("rint(5,1)"), -0.4510083996, 1e-9);
}
