// libs/stats/tests/corr_detrend_test.cpp
//
// Regression guard for corr (Pearson alias) and detrend.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CorrDetrendTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── corr ──────────────────────────────────────────────────

TEST_F(CorrDetrendTest, CorrPerfectLinearColumns)
{
    // X has perfectly correlated columns -> corr is all-ones.
    eval("C = corr([1 2; 2 4; 3 6; 4 8]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,2)"), 1.0);
}

TEST_F(CorrDetrendTest, CorrNegativeCorrelation)
{
    // Anti-correlated columns.
    eval("C = corr([1 4; 2 3; 3 2; 4 1]);");
    EXPECT_NEAR(evalScalar("C(1,2)"), -1.0, 1e-12);
}

TEST_F(CorrDetrendTest, CorrThreeColumns)
{
    eval("C = corr([1 2 3; 4 5 6; 7 8 9; 10 11 12]);");
    // All linearly related -> all 1s.
    EXPECT_DOUBLE_EQ(evalScalar("C(1,3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,3)"), 1.0);
}

TEST_F(CorrDetrendTest, CorrTwoArgRowMismatchRejected)
{
    // Two-arg form added cycle 84. Row-count mismatch still throws.
    EXPECT_THROW(eval("corr([1 2; 3 4; 5 6], [7; 8]);"), std::exception);
    // Sanity: matching rows produce a valid result.
    eval("C = corr([1;2;3;4;5], [2;4;6;8;10]);");
    EXPECT_NEAR(evalScalar("C"), 1.0, 1e-12);
}

// corr 'Type' Spearman/Kendall (was silently ignored -> Pearson). vs MATLAB.
TEST_F(CorrDetrendTest, CorrSpearmanMonotonic)
{
    // Monotonic-but-nonlinear: Pearson<1 but Spearman/Kendall==1.
    eval("x=[1;2;3;4]; y=[1;4;9;16];");
    EXPECT_NEAR(evalScalar("corr(x,y)"),                    0.9843740, 1e-6);
    EXPECT_NEAR(evalScalar("corr(x,y,'type','Spearman')"),  1.0,       1e-12);
    EXPECT_NEAR(evalScalar("corr(x,y,'type','Kendall')"),   1.0,       1e-12);
}

TEST_F(CorrDetrendTest, CorrKendallTauBWithTies)
{
    // tau-b adjusts for ties; Spearman = Pearson of average ranks.
    eval("c=[1;1;2;3;3]; d=[1;2;2;3;4];");
    EXPECT_NEAR(evalScalar("corr(c,d,'type','Spearman')"), 0.892218, 1e-6);
    EXPECT_NEAR(evalScalar("corr(c,d,'type','Kendall')"),  0.824958, 1e-6);
    // Non-monotonic: Kendall differs from Spearman.
    eval("a=[1;2;3;4;5]; b=[2;1;4;3;5];");
    EXPECT_NEAR(evalScalar("corr(a,b,'type','Kendall')"),  0.6, 1e-12);
    EXPECT_NEAR(evalScalar("corr(a,b,'type','Spearman')"), 0.8, 1e-12);
}

TEST_F(CorrDetrendTest, CorrSpearmanMatrixAndCaseInsensitive)
{
    eval("M=[1 2; 2 1; 3 4; 4 3; 5 5];");
    EXPECT_NEAR(evalScalar("R=corr(M,'type','Spearman'); R(1,2)"), 0.8, 1e-12);
    EXPECT_NEAR(evalScalar("Rk=corr(M,'type','Kendall'); Rk(1,1)"), 1.0, 1e-12);
    // Case-insensitive option name + value.
    EXPECT_NEAR(evalScalar("corr([1;2;3;4],[1;4;9;16],'Type','SPEARMAN')"), 1.0, 1e-12);
    // Unknown type errors.
    EXPECT_THROW(eval("corr([1;2;3],[1;2;3],'type','bogus');"), std::exception);
}

// ── detrend ───────────────────────────────────────────────

TEST_F(CorrDetrendTest, DetrendLinearRemovesPerfectly)
{
    // y = 2x + 5: linear trend, detrend should give zeros.
    eval("y = (1:10)' * 2 + 5; yd = detrend(y);");
    EXPECT_NEAR(evalScalar("max(abs(yd))"), 0.0, 1e-12);
}

TEST_F(CorrDetrendTest, DetrendQuadraticRemovesPerfectly)
{
    // y = x^2: order-2 detrend should give zeros.
    eval("y = ((1:10)').^2; yd = detrend(y, 2);");
    EXPECT_NEAR(evalScalar("max(abs(yd))"), 0.0, 1e-10);
}

TEST_F(CorrDetrendTest, DetrendConstantOnly)
{
    // detrend(x, 0) = subtract mean.
    eval("y = [1 2 3 4 5]; yd = detrend(y, 0);");
    EXPECT_NEAR(evalScalar("yd(1)"), -2.0, 1e-12);
    EXPECT_NEAR(evalScalar("yd(3)"),  0.0, 1e-12);
    EXPECT_NEAR(evalScalar("yd(5)"),  2.0, 1e-12);
}

TEST_F(CorrDetrendTest, DetrendStringMode)
{
    eval("y = [1 2 3 4 5]; yd1 = detrend(y, 'constant'); yd2 = detrend(y, 'linear');");
    EXPECT_NEAR(evalScalar("yd1(1)"), -2.0, 1e-12);
    EXPECT_NEAR(evalScalar("max(abs(yd2))"), 0.0, 1e-12);
}

// detrend(x, 1, bp) — continuous piecewise-linear detrend with
// breakpoints. Values verified against MATLAB R2025b. Previously numkit
// silently ignored the breakpoint argument (returned the plain linear
// detrend).
TEST_F(CorrDetrendTest, DetrendBreakpointSingle)
{
    eval("x = [1.1 1.8 3.3 3.9 5.2 5.7]; yd = detrend(x, 1, 3);");
    EXPECT_NEAR(evalScalar("yd(1)"),  0.131578947368, 1e-9);
    EXPECT_NEAR(evalScalar("yd(2)"), -0.263157894737, 1e-9);
    EXPECT_NEAR(evalScalar("yd(3)"),  0.142105263158, 1e-9);
    EXPECT_NEAR(evalScalar("yd(6)"), -0.126315789474, 1e-9);
    // differs from the no-breakpoint linear detrend
    eval("yp = detrend(x, 1);");
    EXPECT_GT(evalScalar("abs(yd(1) - yp(1))"), 1e-3);
}

TEST_F(CorrDetrendTest, DetrendBreakpointMultiAndMatrix)
{
    eval("x = [1.1 1.8 3.3 3.9 5.2 5.7]; yd = detrend(x, 1, [2 4]);");
    EXPECT_NEAR(evalScalar("yd(2)"), -0.131428571429, 1e-9);
    EXPECT_NEAR(evalScalar("yd(3)"),  0.262857142857, 1e-9);
    EXPECT_NEAR(evalScalar("yd(6)"), -0.111428571429, 1e-9);
    // matrix: each column detrended independently (along dim 1)
    eval("M = [x(:) x(:)+0.5]; Yd = detrend(M, 1, 3);");
    EXPECT_NEAR(evalScalar("Yd(1,1)"),  0.131578947368, 1e-9);
    EXPECT_NEAR(evalScalar("Yd(2,2)"), -0.263157894737, 1e-9);
}
