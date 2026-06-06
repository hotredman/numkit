// libs/stats/tests/adtest_dwtest_test.cpp
//
// Regression guards for the hypothesis-test cycle of Group A:
//   adtest — Anderson-Darling normality (estimated parameters)
//   dwtest — Durbin-Watson first-order autocorrelation

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class AdDwTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── adtest ──────────────────────────────────────────────────────────

// Normal data → fails to reject (h = 0).
TEST_F(AdDwTest, AdtestNormalDataDoesNotReject)
{
    eval("x = randn(300, 1); [h, p, A2, cv] = adtest(x);");
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 0);
    EXPECT_GT(evalScalar("p"), 0.05);
    EXPECT_LT(evalScalar("A2"), evalScalar("cv"));   // A² below critical
}

// Exponential data → strongly rejects.
TEST_F(AdDwTest, AdtestExponentialDataRejects)
{
    eval("x = -log(rand(200, 1)); [h, p, A2, ~] = adtest(x);");
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 1);
    EXPECT_LT(evalScalar("p"), 0.01);
    EXPECT_GT(evalScalar("A2"), 1.0);   // far above critical 0.752
}

// Default alpha is 0.05; explicit alpha controls decision only (p, A² same).
TEST_F(AdDwTest, AdtestAlphaControlsDecisionOnly)
{
    eval("x = -log(rand(200, 1));"
         "[h1, p1, A1, ~] = adtest(x, 0.05);"
         "[h2, p2, A2, ~] = adtest(x, 0.01);"
         "samep = (p1 == p2); sameA = (A1 == A2);");
    EXPECT_TRUE(evalScalar("samep") > 0.5);
    EXPECT_TRUE(evalScalar("sameA") > 0.5);
}

// Critical value = 0.752 (Stephens 1986, estimated parameters at α=0.05).
TEST_F(AdDwTest, AdtestCriticalValueIs752)
{
    eval("[~, ~, ~, cv] = adtest(randn(100, 1));");
    EXPECT_NEAR(evalScalar("cv"), 0.752, 1e-9);
}

// Tiny sample throws.
TEST_F(AdDwTest, AdtestTooSmallSampleThrows)
{
    EXPECT_THROW(eval("adtest([1; 2; 3]);"), std::exception);
}

// ── dwtest ──────────────────────────────────────────────────────────

// IID residuals → DW near 2, p large.
TEST_F(AdDwTest, DwtestIIDResidualsDWNearTwo)
{
    eval("n = 200; r = randn(n, 1); X = [ones(n, 1), (1:n)'];"
         "[p, dw] = dwtest(r, X);");
    EXPECT_NEAR(evalScalar("dw"), 2.0, 0.3);
    EXPECT_GT(evalScalar("p"), 0.05);
}

// Strongly positively autocorrelated → DW small, p tiny.
TEST_F(AdDwTest, DwtestPositiveAutocorrRejects)
{
    eval("n = 200; r = zeros(n, 1); r(1) = randn();"
         "for i = 2:n; r(i) = 0.8 * r(i-1) + 0.2 * randn(); end;"
         "X = [ones(n, 1), (1:n)'];"
         "[p, dw] = dwtest(r, X);");
    EXPECT_LT(evalScalar("dw"), 1.0);
    EXPECT_LT(evalScalar("p"), 0.01);
}

// DW statistic algebraic identity: dw = sum((r_i - r_{i-1})²) / sum(r_i²).
TEST_F(AdDwTest, DwtestStatMatchesFormula)
{
    eval("n = 50; r = randn(n, 1); X = ones(n, 1);"
         "[~, dw] = dwtest(r, X);"
         "num = sum((r(2:end) - r(1:end-1)).^2);"
         "den = sum(r.^2);"
         "ref = num / den;"
         "err = abs(dw - ref);");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

// Shape mismatch throws.
TEST_F(AdDwTest, DwtestShapeMismatchThrows)
{
    EXPECT_THROW(eval("dwtest(randn(10, 1), randn(5, 2));"),
                 std::exception);
}
