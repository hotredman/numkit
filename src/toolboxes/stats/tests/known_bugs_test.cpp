// toolboxes/stats/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug in bugs/stats/*.md. Disabled until fixed
// (don't break the green baseline); remove `DISABLED_` to turn into a live
// regression guard. Asserts MATLAB R2025b-correct behaviour.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class StatsKnownBug : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// NOTE: anova1 matrix-input bug FIXED — see toolboxes/stats/tests/anova1_test.cpp.

// bugs/stats/movfun-typeclass.md — FIXED (movsum/movprod/movmean promote
// integer/logical input to double). Full guard in movfun_typeclass_test.cpp.
TEST_F(StatsKnownBug, MovfunTypeClass)
{
    eval("y = movsum(int8([3 1 2 5 4]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'double')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 11.0);
    eval("m = movmean(int8([3 1 2 5 4]), 3);");
    EXPECT_NEAR(evalScalar("m(5)"), 4.5, 1e-12);
    eval("l = movsum(logical([1 0 1 1 0]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("l(2)"), 2.0);
}

// bugs/stats/movfun-order-stats.md — FIXED (movmax/movmin preserve class;
// movmedian rounds int half-away, logical->double). Full guard in
// movfun_order_stats_test.cpp.
TEST_F(StatsKnownBug, MovfunOrderStats)
{
    eval("a = movmax(int8([3 1 2 5 4]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("isa(a,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(3)"), 5.0);
    eval("m = movmedian(int8([3 1 2 5 4]), 2);");
    EXPECT_DOUBLE_EQ(evalScalar("isa(m,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(5)"), 5.0);   // 4.5 -> 5
    eval("lg = movmax(logical([1 0 1 1 0]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("islogical(lg)"), 1.0);
}

// bugs/stats/mle-output.md — 2nd output pci (confidence intervals). FIXED
// 2026-06-05 (deep coverage in toolboxes/stats/tests/mle_pci_test.cpp).
TEST_F(StatsKnownBug, MleConfidenceIntervals)
{
    eval("[phat, pci] = mle([2 3 4 5 6 4 3]);");
    EXPECT_NEAR(evalScalar("pci(1,1)"), 2.613054, 1e-5);
}

// bugs/stats/distribution-dispatchers.md — cdf/pdf/icdf/random.
TEST_F(StatsKnownBug, DISABLED_DistributionDispatchers)
{
    EXPECT_NEAR(evalScalar("cdf('Normal', 1, 0, 1)"),       0.841344746, 1e-7);
    EXPECT_NEAR(evalScalar("pdf('Poisson', 2, 3)"),         0.224041808, 1e-7);
    EXPECT_NEAR(evalScalar("icdf('Normal', 0.975, 0, 1)"),  1.959963985, 1e-7);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(random('Normal',0,1,1,3))")), 3);
}

// bugs/stats/isoutlier-gesd.md — generalized ESD test. FIXED 2026-06-05
// (deep coverage in toolboxes/stats/tests/isoutlier_gesd_test.cpp).
TEST_F(StatsKnownBug, IsoutlierGesd)
{
    eval("m = isoutlier([1 2 3 4 5 6 7 8 9 50], 'gesd');");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(m))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(10))"), 1.0);
}

// bugs/stats/smoothdata-methods.md — sgolay (+ lowess/loess).
// (Verify exact smoothed values vs MATLAB when enabling; default window
// heuristic must match.)
TEST_F(StatsKnownBug, DISABLED_SmoothdataSgolay)
{
    eval("y = smoothdata([1 5 2 8 3 9 4 7 2 8 3], 'sgolay');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 11);
}

// bugs/stats/kstest-pvalue.md — exact small-n Kolmogorov distribution. FIXED
// 2026-06-05 (deep coverage in toolboxes/stats/tests/kstest_exact_test.cpp).
TEST_F(StatsKnownBug, KstestPValue)
{
    eval("[h, p, ks, cv] = kstest([-1 0 1 2 -0.5 0.5]);");
    EXPECT_NEAR(evalScalar("ks"), 0.19146246, 1e-6);   // statistic
    EXPECT_NEAR(evalScalar("p"),  0.94998410, 1e-5);   // was numkit 0.9804
    EXPECT_NEAR(evalScalar("cv"), 0.51926000, 1e-4);   // was numkit 0.5544
}

// bugs/stats/friedman.md — Friedman's nonparametric two-way ANOVA missing.
TEST_F(StatsKnownBug, DISABLED_Friedman)
{
    eval("p = friedman([1 2 3; 2 3 4; 3 4 5; 1 3 5], 1);");
    EXPECT_NEAR(evalScalar("p"), 0.018315639, 1e-6);
}

// bugs/stats/corr-pvalue.md — [r,p]=corr 2nd output. FIXED 2026-06-05
// (deep coverage in toolboxes/stats/tests/corr_pvalue_test.cpp).
TEST_F(StatsKnownBug, CorrPValue)
{
    eval("x=[1 2 3 4 5]'; y=[2 1 4 3 6]'; [r,p]=corr(x,y,'type','Kendall');");
    EXPECT_NEAR(evalScalar("r"), 0.6,      1e-6);
    EXPECT_NEAR(evalScalar("p"), 0.233333, 1e-5);
}

// bugs/stats/kstest-pvalue.md — kstest2 p-value (Stephens corrected). FIXED
// 2026-06-05 (deep coverage in toolboxes/stats/tests/kstest_exact_test.cpp).
TEST_F(StatsKnownBug, Kstest2PValue)
{
    eval("[h,p,k]=kstest2([1 2 3 4 5],[2 3 4 5 6 7]);");
    EXPECT_NEAR(evalScalar("k"), 0.3333333, 1e-6);   // statistic
    EXPECT_NEAR(evalScalar("p"), 0.8470543, 1e-5);   // was numkit 0.9223
}

// bugs/stats/dwtest-pvalue.md — exact DW p-value (Imhof). FIXED 2026-06-05
// (deep coverage in toolboxes/stats/tests/dwtest_exact_test.cpp).
TEST_F(StatsKnownBug, DwtestPValue)
{
    eval("[p,dw]=dwtest([1 2 1 3 2 4]', [ones(6,1) (1:6)']);");
    EXPECT_NEAR(evalScalar("dw"), 0.3142857, 1e-6);  // statistic
    EXPECT_LT(evalScalar("p"), 1e-4);                // MATLAB ~0 (was numkit ~0.017)
}

// bugs/stats/mahal-singular.md — mahal must handle a rank-deficient reference.
TEST_F(StatsKnownBug, DISABLED_MahalSingular)
{
    eval("d = mahal([1 1; 2 2], [0 0; 1 1; 2 2; 3 3]);");   // collinear X
    EXPECT_NEAR(evalScalar("d(1)"), 0.9505075, 1e-5);
}

// bugs/stats/pdist-metrics.md — 'seuclidean'/'spearman' + cosine zero-vector.
// FIXED 2026-06-05 (deep coverage in toolboxes/stats/tests/pdist_metrics_test.cpp).
TEST_F(StatsKnownBug, PdistMetrics)
{
    eval("A = [1 2 3; 4 5 7; 1 0 2];");
    // seuclidean: was "unknown metric"; MATLAB max dist 3.2433.
    EXPECT_NEAR(evalScalar("max(pdist(A,'seuclidean'))"), 3.2433, 1e-3);
    EXPECT_NO_THROW(eval("pdist(A,'spearman');"));
    // cosine distance with a zero-norm row should be NaN (MATLAB), not 1.
    EXPECT_TRUE(eval("isnan(pdist([0 0;3 4],'cosine'))").toBool());
}

// bugs/stats/distribution-array-params.md — broadcast array distribution params.
// FIXED 2026-06-05 (cycles 29-38): all 16 *pdf/*cdf/*inv families broadcast
// ARRAY parameters. Deep per-family coverage in
// toolboxes/stats/tests/dist_broadcast_test.cpp (36 TEST_F) + the corr.json-style
// parity spec tools/parity/specs/dist_broadcast.json.
TEST_F(StatsKnownBug, DistributionArrayParams)
{
    eval("y = normpdf(0, 0, [1 2 4]);");          // MATLAB [0.3989 0.1995 0.0997]
    EXPECT_NEAR(evalScalar("y(2)"), 0.19947114, 1e-6);
    eval("b = binopdf(2, [4 5 6], 0.5);");        // MATLAB [0.375 0.3125 0.234375]
    EXPECT_NEAR(evalScalar("b(3)"), 0.234375, 1e-6);
    eval("g = gampdf(1, [1 2 3], 1);");           // MATLAB [0.3679 0.3679 0.1839]
    EXPECT_NEAR(evalScalar("g(3)"), 0.18393972, 1e-6);
}

// bugs/stats/autocorr.md — autocorr/parcorr/crosscorr (Econometrics) missing.
TEST_F(StatsKnownBug, DISABLED_Autocorr)
{
    eval("ac = autocorr([1 2 3 2 1 2 3 2 1]);");   // default NumLags=min(20,N-1)=8
    EXPECT_EQ(static_cast<int>(evalScalar("numel(ac)")), 9);
    EXPECT_NEAR(evalScalar("ac(1)"), 1.0, 1e-12);                // lag 0 == 1
    EXPECT_NEAR(evalScalar("ac(2)"), 0.0202020202020202, 1e-9); // lag 1
    eval("pc = parcorr([1 2 3 2 1 2 3 2 1]);");
    EXPECT_NEAR(evalScalar("pc(1)"), 1.0, 1e-12);   // PACF lag 0 == 1
    // crosscorr zero-lag of x and reversed x == -1 (perfectly anti-correlated)
    eval("xc = crosscorr([1 2 3 4],[4 3 2 1],'NumLags',2);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(xc)")), 5);
    EXPECT_NEAR(evalScalar("xc(3)"), -1.0, 1e-9);
}

// NOTE: combnk scalar-set bug FIXED — see toolboxes/stats/tests/combnk_test.cpp.
