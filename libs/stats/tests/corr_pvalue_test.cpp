// libs/stats/tests/corr_pvalue_test.cpp
//
// Regression guard for bugs/stats/corr-pvalue.md (FIXED): [r, p] = corr(...)
// now emits the p-value (test of H0: no association). Pearson via the
// t-distribution; Kendall via the EXACT permutation (Mahonian inversions)
// distribution; Spearman via the EXACT permutation distribution for small n.
// The p matrix matches r's shape; for the corr(X) auto-correlation form the
// diagonal is 1. MATLAB R2025b reference. (Large-n Spearman uses a
// t-approximation -- MATLAB's AS 89 -- so only small-n exact is asserted.)

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class CorrPValueTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Pearson (default): t-distribution p-value.
TEST_F(CorrPValueTest, Pearson)
{
    eval("x=[1 2 3 4 5]'; y=[2 1 4 3 6]'; [r, p] = corr(x, y);");
    EXPECT_NEAR(evalScalar("r"), 0.821995, 1e-5);
    EXPECT_NEAR(evalScalar("p"), 0.087706647, 1e-7);
}

// Spearman: exact permutation p (small n).
TEST_F(CorrPValueTest, SpearmanExact)
{
    eval("x=[1 2 3 4 5]'; y=[2 1 4 3 6]'; [r, p] = corr(x, y, 'type', 'Spearman');");
    EXPECT_NEAR(evalScalar("r"), 0.8, 1e-9);
    EXPECT_NEAR(evalScalar("p"), 0.13333333, 1e-7);
    eval("[~, p7] = corr([1 2 3 4 5 6 7]', [2 1 4 3 6 5 7]', 'type', 'Spearman');");
    EXPECT_NEAR(evalScalar("p7"), 0.012301587, 1e-7);
}

// Kendall: exact inversions-distribution p.
TEST_F(CorrPValueTest, KendallExact)
{
    eval("x=[1 2 3 4 5]'; y=[2 1 4 3 6]'; [r, p] = corr(x, y, 'type', 'Kendall');");
    EXPECT_NEAR(evalScalar("r"), 0.6, 1e-9);
    EXPECT_NEAR(evalScalar("p"), 0.23333333, 1e-7);
    eval("[~, p8] = corr([1 2 3 4 5 6 7 8]', [2 1 4 3 6 5 8 7]', 'type', 'Kendall');");
    EXPECT_NEAR(evalScalar("p8"), 0.014136905, 1e-7);
}

// Matrix form corr(X): p has r's shape; diagonal = 1 (auto-correlation).
TEST_F(CorrPValueTest, MatrixDiagonalOne)
{
    eval("X = [1 2 3 4 5; 2 1 4 3 6; 1 3 2 5 4]'; [R, P] = corr(X);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,2)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("P(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(3,3)"), 1.0);
    EXPECT_NEAR(evalScalar("P(1,2)"), 0.087706647, 1e-7);   // = the Pearson p
    EXPECT_NEAR(evalScalar("P(2,1)"), 0.087706647, 1e-7);   // symmetric
}

// The 1-output form is unchanged.
TEST_F(CorrPValueTest, OneOutputUnchanged)
{
    eval("r = corr([1 2 3 4 5]', [2 1 4 3 6]');");
    EXPECT_NEAR(evalScalar("r"), 0.821995, 1e-5);
}
