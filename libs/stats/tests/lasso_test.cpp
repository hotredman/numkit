// libs/stats/tests/lasso_test.cpp
//
// Regression guard for lasso + lassoglm.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class LassoTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── lasso ───────────────────────────────────────────────────────────

// At λ = 0, lasso recovers OLS exactly (no regularisation).
TEST_F(LassoTest, ZeroLambdaMatchesOLS)
{
    eval("n = 100; X = randn(n, 3); y = X * [1; -1; 2] + 0.1 * randn(n, 1);"
         "[B, intercept, ~] = lasso(X, y, 0, 1.0);"
         "X1 = [ones(n, 1), X]; b_ols = regress(y, X1, 0.05);"
         "err = max(abs(B - b_ols(2:end)));");
    EXPECT_LT(evalScalar("err"), 0.05);
}

// Sparse signal recovery: at moderate λ, zero coefficients exactly.
TEST_F(LassoTest, RecoversSparsePattern)
{
    eval("n = 300; p = 5; X = randn(n, p);"
         "beta_true = [2.0; 0.0; 3.0; 0.0; 0.0];"
         "y = X * beta_true + 0.3 * randn(n, 1);"
         "[B, ~, ~] = lasso(X, y, 0.1, 1.0);"
         // β1, β3 should be near true; β2, β4, β5 should be exactly 0.
         "n_zero = sum(abs(B(2:2:end)) < 1e-9);");
    EXPECT_NEAR(evalScalar("B(1)"), 2.0, 0.3);
    EXPECT_NEAR(evalScalar("B(3)"), 3.0, 0.3);
    EXPECT_DOUBLE_EQ(evalScalar("B(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(5)"), 0.0);
}

// Multi-λ path: shape and ordering invariants.
TEST_F(LassoTest, MultiLambdaPathShape)
{
    eval("n = 100; p = 4; X = randn(n, p);"
         "y = X * [1; 0; 2; 0] + 0.2 * randn(n, 1);"
         "lambdas = [0.01, 0.1, 0.5, 1.0];"
         "[B, intercepts, ~] = lasso(X, y, lambdas, 1.0);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 2)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(intercepts)")), 4);
    // Coefficient magnitude should shrink with larger λ.
    EXPECT_GT(evalScalar("abs(B(1, 1))"), evalScalar("abs(B(1, 4))"));
}

// Elastic net (α = 0.5): no zeros in B due to L2 ridge term.
TEST_F(LassoTest, ElasticNetSmoothsCoefficients)
{
    eval("n = 100; X = randn(n, 5);"
         "y = X * [1; 0; 2; 0; 0] + 0.1 * randn(n, 1);"
         "[Bl, ~, ~] = lasso(X, y, 0.1, 1.0);"     // pure lasso
         "[Be, ~, ~] = lasso(X, y, 0.1, 0.5);"     // elastic net
         "nz_lasso = sum(Bl == 0);"
         "nz_en    = sum(Be == 0);");
    // Lasso → more exact zeros than EN.
    EXPECT_GE(evalScalar("nz_lasso"), evalScalar("nz_en"));
}

TEST_F(LassoTest, LassoEmptyLambdasThrows)
{
    EXPECT_THROW(eval("lasso(randn(10, 3), randn(10, 1), [], 1.0);"),
                 std::exception);
}

// ── lassoglm ────────────────────────────────────────────────────────

TEST_F(LassoTest, LassoglmLogisticSparseRecovery)
{
    eval("n = 500; X = randn(n, 4);"
         "eta = 0.5 * X(:, 1) + 1.5 * X(:, 3);"
         "y = double(rand(n, 1) < 1 ./ (1 + exp(-eta)));"
         "[B, intercepts, ~] = lassoglm(X, y, 'binomial', 0.05, 1.0);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 4);
    EXPECT_GT(evalScalar("abs(B(1))"), 0.0);
    EXPECT_GT(evalScalar("abs(B(3))"), 0.0);
    // X2, X4 should be heavily shrunk (often exact zero at this λ).
    EXPECT_LT(evalScalar("abs(B(2))"), 0.5);
    EXPECT_LT(evalScalar("abs(B(4))"), 0.5);
}

TEST_F(LassoTest, LassoglmReturnsLambdaPath)
{
    eval("n = 200; X = randn(n, 3); y = double(X(:, 1) > 0);"
         "lambdas = [0.001, 0.05, 0.2];"
         "[B, ~, lam] = lassoglm(X, y, 'binomial', lambdas, 1.0);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(lam)")), 3);
}
