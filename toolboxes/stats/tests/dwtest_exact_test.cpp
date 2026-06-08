// toolboxes/stats/tests/dwtest_exact_test.cpp
//
// Regression guard for bugs/stats/dwtest-pvalue.md (FIXED): dwtest computes the
// EXACT Durbin-Watson p-value via Imhof's characteristic-function inversion
// (the residual-space eigenvalues of MAM), matching MATLAB's default 'exact'
// method, plus the 'Tail' option ('both' default / 'right' / 'left'). The DW
// statistic was already correct. MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <algorithm>

class DwtestExactTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Strong positive autocorrelation (low DW): two-sided p ≈ 0, dw = 11/35.
TEST_F(DwtestExactTest, LowDwBothTail)
{
    eval("[p,dw] = dwtest([1 2 1 3 2 4]', [ones(6,1) (1:6)']);");
    EXPECT_NEAR(evalScalar("dw"), 0.3142857142857143, 1e-9);
    EXPECT_NEAR(evalScalar("p"),  0.0, 1e-6);
    EXPECT_NEAR(evalScalar("dwtest([1 2 1 3 2 4]', [ones(6,1) (1:6)'], 'Tail', 'right')"), 0.0, 1e-6);
    EXPECT_NEAR(evalScalar("dwtest([1 2 1 3 2 4]', [ones(6,1) (1:6)'], 'Tail', 'left')"),  1.0, 1e-6);
}

// Negative autocorrelation (high DW): well-spread exact tail probabilities.
// MATLAB: dw=3.5, both=0.005693520875, right=0.9971532396, left=0.002846760438.
TEST_F(DwtestExactTest, HighDwAllTails)
{
    eval("r = [1 -1 1 -1 1 -1 1 -1]'; X = ones(8,1);");
    eval("[p,dw] = dwtest(r, X);");
    EXPECT_NEAR(evalScalar("dw"), 3.5, 1e-9);
    EXPECT_NEAR(evalScalar("p"),  0.005693520875, 1e-7);
    EXPECT_NEAR(evalScalar("dwtest(r, X, 'Tail', 'right')"), 0.9971532396, 1e-7);
    EXPECT_NEAR(evalScalar("dwtest(r, X, 'Tail', 'left')"),  0.002846760438, 1e-7);
}

// A second low-DW design.
TEST_F(DwtestExactTest, SecondDesign)
{
    eval("[p,dw] = dwtest([2 1 4 3 6 5 8 7]', [ones(8,1) (1:8)']);");
    EXPECT_NEAR(evalScalar("dw"), 0.15196078431372548, 1e-9);
    EXPECT_NEAR(evalScalar("p"),  0.0, 1e-6);
}

// 'both' = 2*min(left-area, right-area); right + left areas sum to 1.
TEST_F(DwtestExactTest, TailRelations)
{
    eval("r = [1 -1 1 -1 1 -1 1 -1]'; X = ones(8,1);");
    const double pr = evalScalar("dwtest(r, X, 'Tail', 'right')");
    const double pl = evalScalar("dwtest(r, X, 'Tail', 'left')");
    EXPECT_NEAR(pr + pl, 1.0, 1e-9);
    EXPECT_NEAR(evalScalar("dwtest(r, X)"), 2.0 * std::min(pr, pl), 1e-9);
}

// The 'approximate' method is accepted (numkit's own beta approximation — not
// MATLAB-identical; the default exact method is the MATLAB-matching path).
TEST_F(DwtestExactTest, ApproximateMethodRuns)
{
    EXPECT_NO_THROW(eval("dwtest([1 2 1 3 2 4]', [ones(6,1) (1:6)'], 'Method', 'approximate');"));
    // a bad option value is rejected
    EXPECT_ANY_THROW(eval("dwtest([1 2 1 3 2 4]', [ones(6,1) (1:6)'], 'Tail', 'sideways');"));
}
