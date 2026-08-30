// toolboxes/stats/tests/nancov_test.cpp
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
    StandardEngine engine;
    void SetUp() override {}
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

// ── corrcoef 'Rows' NaN policy (2026-05-30): 'complete' / 'pairwise' ──
// corrcoef previously ERRORED on 'complete'/'pairwise' (the string fell
// through cov's two-input path) and NaN-poisoned the two-vector form.
// vs MATLAB R2025b.
TEST_F(NancovTest, CorrcoefRowsComplete)
{
    eval("Xn = [1 5 2; 2 6 9; 3 NaN 4; 4 8 1; 5 9 NaN];");
    eval("Cc = corrcoef(Xn,'Rows','complete');");
    EXPECT_NEAR(evalScalar("Cc(1,2)"),  1.0,                1e-12);
    EXPECT_NEAR(evalScalar("Cc(1,3)"), -0.300375704593055, 1e-10);
    EXPECT_NEAR(evalScalar("Cc(3,3)"),  1.0,                1e-12);
    // Two-vector complete.
    EXPECT_NEAR(evalScalar("v = corrcoef([1;2;3;NaN;5],[2;4;6;8;NaN],'Rows','complete'); v(1,2)"),
                1.0, 1e-12);
    // [R,P] complete: P uses the complete-row count.
    eval("[R,P] = corrcoef(Xn,'Rows','complete');");
    EXPECT_NEAR(evalScalar("P(1,3)"), 0.805776, 1e-5);
}

TEST_F(NancovTest, CorrcoefRowsPairwise)
{
    eval("Xn = [1 5 2; 2 6 9; 3 NaN 4; 4 8 1; 5 9 NaN];");
    eval("Cp = corrcoef(Xn,'Rows','pairwise');");
    // Each (i,j) normalized over its own common rows -> entries can differ.
    EXPECT_NEAR(evalScalar("Cp(1,3)"), -0.290190500044005, 1e-10);
    EXPECT_NEAR(evalScalar("Cp(2,3)"), -0.300375704593055, 1e-10);
    EXPECT_NEAR(evalScalar("Cp(1,1)"),  1.0,               1e-12);
    // 'all' (default) still NaN-poisons.
    EXPECT_TRUE(std::isnan(evalScalar("Ca = corrcoef(Xn); Ca(2,2)")));
    // [R,P] with 'pairwise' is deferred (per-pair df) -> clear error.
    EXPECT_THROW(eval("[R,P] = corrcoef(Xn,'Rows','pairwise');"), std::exception);
}

// ── skewness/kurtosis NaN omission (2026-05-30) ──────────────────────
// MATLAB skewness/kurtosis treat NaN as missing and remove it per
// column; numkit previously NaN-poisoned. vs MATLAB R2025b.
TEST_F(NancovTest, SkewnessKurtosisOmitNaN)
{
    eval("Mn = [1 5; 2 NaN; 3 7; 4 100];");
    // Column 2 = [5;7;100] after dropping the NaN.
    EXPECT_NEAR(evalScalar("s = skewness(Mn); s(2)"), 0.706027, 1e-5);
    EXPECT_NEAR(evalScalar("k = kurtosis(Mn); k(2)"), 1.5,      1e-9);
    // Column 1 (no NaN) is unchanged.
    EXPECT_NEAR(evalScalar("s = skewness(Mn); s(1)"), 0.0,      1e-12);
    EXPECT_NEAR(evalScalar("k = kurtosis(Mn); k(1)"), 1.64,     1e-9);
    // Clean data is unaffected by the change.
    EXPECT_NEAR(evalScalar("s = skewness([1 5; 2 6; 3 7; 4 100]); s(2)"),
                1.153657, 1e-5);
    // A column with < 2 non-NaN values yields NaN.
    EXPECT_TRUE(std::isnan(evalScalar("s = skewness([NaN 5; NaN 6; 3 7]); s(1)")));
    // flag=0 (bias-corrected) kurtosis needs >= 4 non-NaN -> NaN with 3.
    EXPECT_TRUE(std::isnan(evalScalar("k = kurtosis(Mn,0); k(2)")));
}

// ── mad/iqr NaN omission (2026-05-30) ────────────────────────────────
// MATLAB mad and iqr treat NaN as missing and remove it per column;
// numkit previously NaN-poisoned. vs MATLAB R2025b.
TEST_F(NancovTest, MadIqrOmitNaN)
{
    eval("Mn = [1 5; 2 NaN; 3 7; 4 100];");
    // Column 2 = [5;7;100] after dropping the NaN.
    EXPECT_NEAR(evalScalar("m = mad(Mn);   m(2)"), 41.77777777778, 1e-9);
    EXPECT_NEAR(evalScalar("m = mad(Mn,1); m(2)"),  2.0,           1e-12);
    EXPECT_NEAR(evalScalar("q = iqr(Mn);   q(2)"), 71.25,          1e-12);
    // Column 1 (no NaN) unchanged.
    EXPECT_NEAR(evalScalar("m = mad(Mn);   m(1)"),  1.0,           1e-12);
    EXPECT_NEAR(evalScalar("q = iqr(Mn);   q(1)"),  2.0,           1e-12);
    // Vector with an interior NaN.
    EXPECT_NEAR(evalScalar("mad([1 2 NaN 4 100])"), 36.625,        1e-9);
    EXPECT_NEAR(evalScalar("iqr([1 2 NaN 4 100])"), 50.5,          1e-12);
    // Clean data is unaffected.
    EXPECT_NEAR(evalScalar("m = mad([1 5;2 6;3 7;4 100]); m(2)"), 35.25, 1e-12);
    EXPECT_NEAR(evalScalar("q = iqr([1 5;2 6;3 7;4 100]); q(2)"), 48.0,  1e-12);
    // All-NaN column -> NaN.
    EXPECT_TRUE(std::isnan(evalScalar("m = mad([NaN; NaN; NaN]); m(1)")));
    EXPECT_TRUE(std::isnan(evalScalar("q = iqr([NaN; NaN; NaN]); q(1)")));
}

// ── geomean/harmmean 'omitnan' nanflag (2026-05-30) ──────────────────
// MATLAB geomean/harmmean accept a trailing 'omitnan' nanflag that
// removes NaN per slice; numkit previously ignored it (geomean -> NaN)
// or errored converting the char to a dim (harmmean). vs MATLAB R2025b.
TEST_F(NancovTest, GeomeanHarmmeanOmitNaN)
{
    eval("Mn = [1 5; 2 NaN; 3 7; 4 100; 5 8];");
    // Column 2 = [5;7;100;8] after dropping the NaN.
    EXPECT_NEAR(evalScalar("g = geomean(Mn,1,'omitnan');  g(2)"), 12.935687, 1e-5);
    EXPECT_NEAR(evalScalar("g = geomean(Mn,'omitnan');     g(2)"), 12.935687, 1e-5);
    EXPECT_NEAR(evalScalar("h = harmmean(Mn,'omitnan');    h(2)"),  8.3707025, 1e-6);
    EXPECT_NEAR(evalScalar("h = harmmean(Mn,1,'omitnan');  h(2)"),  8.3707025, 1e-6);
    // Column 1 (no NaN) unchanged.
    EXPECT_NEAR(evalScalar("g = geomean(Mn,'omitnan');  g(1)"), 2.605171084697, 1e-9);
    // Default (includenan) still propagates NaN.
    EXPECT_TRUE(std::isnan(evalScalar("g = geomean(Mn); g(2)")));
    EXPECT_TRUE(std::isnan(evalScalar("h = harmmean(Mn); h(2)")));
    // omitnan honours the dim argument.
    EXPECT_NEAR(evalScalar("g = geomean([1 NaN 3; 4 5 6],2,'omitnan'); g(1)"),
                1.732050807568877, 1e-9);
    // An unknown option is an error.
    EXPECT_THROW(eval("geomean(Mn,'foo');"), std::exception);
}

// ── mode 3rd output C (2026-05-30): cell of tied modal values ────────
// [M,F,C]=mode(X). C is a cell array; each cell holds a sorted column
// vector of the values tied for the modal frequency in that slice.
// Previously numkit only returned [M,F]. vs MATLAB R2025b.
TEST_F(NancovTest, ModeThirdOutputCell)
{
    // Vector: 2 and 3 both occur twice -> M=2 (smallest tie), C{1}=[2;3].
    eval("[m,f,c] = mode([3 3 1 2 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("m"),           2.0);
    EXPECT_DOUBLE_EQ(evalScalar("f"),           2.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(c{1})"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{1}(1)"),     2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{1}(2)"),     3.0);
    // Matrix, default (per column): col1 ties 1&2 -> C{1}=[1;2]; col2 -> [2].
    eval("M = [1 1; 2 2; 1 3; 2 2]; [mm,fm,cm] = mode(M);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(cm{1})"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("cm{1}(1)"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("cm{1}(2)"),     2.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(cm{2})"), 1.0);
    // dim=2 (per row): row3 = [1 3], both once -> C{3}=[1;3].
    eval("[mr,fr,cr] = mode(M,2);");
    EXPECT_DOUBLE_EQ(evalScalar("cr{3}(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("cr{3}(2)"), 3.0);
    // 'all' flatten.
    eval("[ma,fa,ca] = mode(M,'all');");
    EXPECT_DOUBLE_EQ(evalScalar("ca{1}(1)"), 2.0);
}
