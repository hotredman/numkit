// libs/stats/tests/nancov_test.cpp
//
// Regression guard for nancov() — NaN-aware covariance (MATLAB legacy
// stats fn; equivalent to cov(X, 'omitrows')). v1 implements 'complete'
// mode only ('pairwise' is a documented gap).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class NancovTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Single-input form, with one row containing NaN.
TEST_F(NancovTest, MatrixDropsNaNRowMatchesCovOfClean)
{
    eval("X = [1 2; 3 4; NaN 6; 5 8]; C = nancov(X);"
         "Xc = [1 2; 3 4; 5 8]; Cref = cov(Xc);"
         "err = max(max(abs(C - Cref)));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(C, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(C, 2)")), 2);
    EXPECT_LT(evalScalar("err"), 1e-12);
    EXPECT_NEAR(evalScalar("C(1, 1)"), 4.0,         1e-12);
    EXPECT_NEAR(evalScalar("C(2, 2)"), 28.0 / 3.0,  1e-12);
}

// Normalization flag: 0 → (n-1) divisor, 1 → n.
TEST_F(NancovTest, NormalizationFlagSwitchesDivisor)
{
    eval("X = [1 2; 3 4; NaN 6; 5 8];"
         "C0 = nancov(X, 0); C1 = nancov(X, 1);"
         "ratio = C0(1, 1) / C1(1, 1);");
    // After dropping NaN row: 3 obs → ratio = (n-1)/n · (n/n-1) = n/(n-1) = 3/2.
    EXPECT_NEAR(evalScalar("ratio"), 1.5, 1e-12);
}

// Two-vector form: nancov(x, y) treats [x y] as 2-col matrix.
TEST_F(NancovTest, TwoVectorFormReturnsTwoByTwo)
{
    eval("v1 = [1; 2; NaN; 4; 5];"
         "v2 = [10; 20; 30; NaN; 50];"
         "C = nancov(v1, v2);");
    // Rows 3 (NaN in v1) and 4 (NaN in v2) drop → kept = [1 10; 2 20; 5 50].
    // mean = [8/3 80/3] ; deviations [(−5/3,−50/3), (−2/3,−20/3), (7/3,70/3)]
    // sum sq dev x = 25/9+4/9+49/9 = 78/9 ; var (n-1) = 78/9 / 2 = 13/3
    // sum sq dev y = 7800/9 ; var = 1300/3
    EXPECT_EQ(static_cast<int>(evalScalar("size(C, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(C, 2)")), 2);
    EXPECT_NEAR(evalScalar("C(1, 1)"), 13.0 / 3.0,  1e-12);
    EXPECT_NEAR(evalScalar("C(2, 2)"), 1300.0 / 3.0, 1e-12);
}

// Vector input → scalar variance.
TEST_F(NancovTest, VectorInputReturnsScalarVariance)
{
    eval("v = [1; 2; NaN; 4; 5]; vRef = [1; 2; 4; 5];"
         "c = nancov(v); vr = var(vRef);"
         "err = abs(c - vr);");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

// All-NaN input → NaN (no observations to estimate from).
TEST_F(NancovTest, AllNaNRowsReturnNaNMatrix)
{
    eval("X = [NaN 1; 2 NaN]; C = nancov(X);"
         "allnan = all(isnan(C(:)));");
    EXPECT_TRUE(evalScalar("allnan") > 0.5);
}

// ── cov NaN-policy flag (2026-05-30): 'omitrows' / 'partialrows' ──────
// Previously cov(X,'omitrows') fell through to the two-input matrix path
// and errored. cov now accepts a trailing 'includenan'|'omitrows'|
// 'partialrows' flag. vs MATLAB R2025b.
TEST_F(NancovTest, OmitrowsDropsNaNRows)
{
    eval("X = [1 5; 2 6; 3 NaN; 4 8];");
    eval("Co = cov(X,'omitrows');");
    EXPECT_NEAR(evalScalar("Co(1,1)"), 7.0 / 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("Co(1,2)"), 7.0 / 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("Co(2,2)"), 7.0 / 3.0, 1e-12);
    // N normalization variant.
    EXPECT_NEAR(evalScalar("Cw = cov(X,1,'omitrows'); Cw(1,1)"), 14.0 / 9.0, 1e-12);
    // Vector input reduces to variance over the non-NaN elements.
    EXPECT_NEAR(evalScalar("cov([1 2 NaN 4],'omitrows')"), 7.0 / 3.0, 1e-12);
}

TEST_F(NancovTest, PartialrowsPairwiseDeletion)
{
    eval("X = [1 5; 2 6; 3 NaN; 4 8];");
    eval("Cp = cov(X,'partialrows');");
    // cov(1,1): all 4 rows of col1 -> var([1 2 3 4]) = 5/3.
    EXPECT_NEAR(evalScalar("Cp(1,1)"), 5.0 / 3.0, 1e-12);
    // cov(1,2) and cov(2,2): rows valid for both/col2 = {1,2,4} -> 7/3.
    EXPECT_NEAR(evalScalar("Cp(1,2)"), 7.0 / 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("Cp(2,1)"), 7.0 / 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("Cp(2,2)"), 7.0 / 3.0, 1e-12);
    // 'includenan' (default) still NaN-poisons.
    EXPECT_TRUE(std::isnan(evalScalar("Ci = cov(X,'includenan'); Ci(2,2)")));
}
