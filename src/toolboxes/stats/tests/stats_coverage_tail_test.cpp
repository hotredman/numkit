// toolboxes/stats/tests/stats_coverage_tail_test.cpp
//
// Closes the last public-function gtest gaps in toolboxes/stats that were
// previously parity-only:
//   regression   lscov, ridge
//   descriptive  datastats, dummyvar
//   cluster      rangesearch
//   distributions gevrnd, mnpdf, mvtpdf
// (jackknife is a separate stub defect — see bugs/stats/jackknife.md.)
// Reference values verified against the engine; mnpdf has a closed form
// (6!/(1!2!3!)*.2*.09*.125 = 0.135), mvtpdf(0,I,5) = 1/(2*pi).

#include "dual_engine_fixture.hpp"

using namespace m_test;

class StatsCoverageTailTest : public DualEngineTest
{};

// ── lscov: weighted least squares + standard errors ─────────────────────
TEST_P(StatsCoverageTailTest, Lscov)
{
    // Fit b = x1 + x2*t to (t,b) = (1,1)(2,3)(3,4)(4,5): slope 1.3, intercept 0.
    eval("A = [1 1; 1 2; 1 3; 1 4]; b = [1; 3; 4; 5];");
    eval("x = lscov(A, b);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.0, 1e-10);
    EXPECT_NEAR(evalScalar("x(2)"), 1.3, 1e-10);
    // Uniform weights reduce to OLS.
    eval("xw = lscov(A, b, [1;1;1;1]);");
    EXPECT_NEAR(evalScalar("xw(2)"), 1.3, 1e-10);
    // Second output = parameter standard errors.
    eval("[~, se] = lscov(A, b);");
    EXPECT_NEAR(evalScalar("se(1)"), 0.4743416490, 1e-7);
    EXPECT_NEAR(evalScalar("se(2)"), 0.1732050808, 1e-7);
}

// ── ridge: ridge regression coefficients (scaled = 0 → original scale) ──
TEST_P(StatsCoverageTailTest, Ridge)
{
    eval("X = [1 2; 2 1; 3 4; 4 3; 5 6]; y = [2; 3; 6; 7; 11];");
    // k = 0 reduces to OLS-with-intercept (3 coefs for 2 predictors).
    eval("b0 = ridge(y, X, 0, 0);");
    EXPECT_EQ(eval("b0").numel(), 3u);
    EXPECT_NEAR(evalScalar("b0(1)"), -0.933333, 1e-5);
    EXPECT_NEAR(evalScalar("b0(2)"), 1.533333, 1e-5);
    EXPECT_NEAR(evalScalar("b0(3)"), 0.666667, 1e-5);
    // k > 0 shrinks coefficients toward zero.
    eval("bk = ridge(y, X, 10, 0);");
    EXPECT_NEAR(evalScalar("bk(1)"), 3.008873, 1e-5);
    EXPECT_NEAR(evalScalar("bk(2)"), 0.518155, 1e-5);
    EXPECT_NEAR(evalScalar("bk(3)"), 0.386457, 1e-5);
}

// ── datastats: summary struct of a data vector ──────────────────────────
TEST_P(StatsCoverageTailTest, Datastats)
{
    eval("s = datastats([3 1 4 1 5 9 2 6]);");
    EXPECT_DOUBLE_EQ(evalScalar("s.num"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.min"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.max"), 9.0);
    EXPECT_NEAR(evalScalar("s.mean"), 3.875, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("s.median"), 3.5);
    EXPECT_DOUBLE_EQ(evalScalar("s.range"), 8.0);
}

// ── dummyvar: group labels → indicator columns ──────────────────────────
TEST_P(StatsCoverageTailTest, Dummyvar)
{
    eval("D = dummyvar([1 2 3 2 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(D, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(D, 2)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("D(1,1)"), 1.0);   // obs 1 -> group 1
    EXPECT_DOUBLE_EQ(evalScalar("D(2,2)"), 1.0);   // obs 2 -> group 2
    EXPECT_DOUBLE_EQ(evalScalar("D(3,3)"), 1.0);   // obs 3 -> group 3
    EXPECT_DOUBLE_EQ(evalScalar("D(5,1)"), 1.0);   // obs 5 -> group 1
    EXPECT_DOUBLE_EQ(evalScalar("sum(D(:))"), 5.0);  // exactly one 1 per row
}

// ── rangesearch: neighbors within a radius (cell of index rows) ─────────
TEST_P(StatsCoverageTailTest, Rangesearch)
{
    eval("X = [0 0; 1 0; 2 0; 5 0]; idx = rangesearch(X, [0 0], 1.5);");
    EXPECT_DOUBLE_EQ(evalScalar("iscell(idx)"), 1.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(idx{1})")), 2);  // points 1 and 2
    EXPECT_DOUBLE_EQ(evalScalar("idx{1}(1)"), 1.0);               // nearest first
    EXPECT_DOUBLE_EQ(evalScalar("idx{1}(2)"), 2.0);
}

// ── distributions: gevrnd shape/mean, mnpdf + mvtpdf closed forms ───────
TEST_P(StatsCoverageTailTest, GevrndShapeAndMean)
{
    eval("rng(0); r = gevrnd(0.1, 1, 0, 2000, 1); mu = mean(r);");
    EXPECT_EQ(eval("r").numel(), 2000u);
    EXPECT_NEAR(evalScalar("mu"), 0.686, 0.3);   // population mean (gamma(0.9)-1)/0.1
}

TEST_P(StatsCoverageTailTest, MnpdfMvtpdf)
{
    EXPECT_NEAR(evalScalar("mnpdf([1 2 3], [0.2 0.3 0.5])"), 0.135, 1e-12);
    EXPECT_NEAR(evalScalar("mvtpdf([0 0], eye(2), 5)"), 0.1591549431, 1e-9);  // 1/(2*pi)
}

INSTANTIATE_DUAL(StatsCoverageTailTest);
