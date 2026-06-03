// libs/stats/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug in bugs/stats/*.md. Disabled until fixed
// (don't break the green baseline); remove `DISABLED_` to turn into a live
// regression guard. Asserts MATLAB R2025b-correct behaviour.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class StatsKnownBug : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// NOTE: anova1 matrix-input bug FIXED — see libs/stats/tests/anova1_test.cpp.

// bugs/stats/mle-output.md — 2nd output pci (confidence intervals).
TEST_F(StatsKnownBug, DISABLED_MleConfidenceIntervals)
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

// bugs/stats/isoutlier-gesd.md — generalized ESD test.
TEST_F(StatsKnownBug, DISABLED_IsoutlierGesd)
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

// bugs/stats/kstest-pvalue.md — statistic OK, p-value/critical value wrong
// (numkit asymptotic vs MATLAB exact small-n Kolmogorov distribution).
TEST_F(StatsKnownBug, DISABLED_KstestPValue)
{
    eval("[h, p, ks, cv] = kstest([-1 0 1 2 -0.5 0.5]);");
    EXPECT_NEAR(evalScalar("ks"), 0.19146246, 1e-6);   // statistic already OK
    EXPECT_NEAR(evalScalar("p"),  0.94998410, 1e-5);   // numkit currently 0.9804
    EXPECT_NEAR(evalScalar("cv"), 0.51926000, 1e-4);   // numkit currently 0.5544
}

// bugs/stats/friedman.md — Friedman's nonparametric two-way ANOVA missing.
TEST_F(StatsKnownBug, DISABLED_Friedman)
{
    eval("p = friedman([1 2 3; 2 3 4; 3 4 5; 1 3 5], 1);");
    EXPECT_NEAR(evalScalar("p"), 0.018315639, 1e-6);
}

// bugs/stats/corr-pvalue.md — [r,p]=corr missing the p-value 2nd output.
TEST_F(StatsKnownBug, DISABLED_CorrPValue)
{
    eval("x=[1 2 3 4 5]'; y=[2 1 4 3 6]'; [r,p]=corr(x,y,'type','Kendall');");
    EXPECT_NEAR(evalScalar("r"), 0.6,      1e-6);
    EXPECT_NEAR(evalScalar("p"), 0.233333, 1e-5);
}

// bugs/stats/kstest-pvalue.md — kstest2 p-value wrong (statistic correct).
TEST_F(StatsKnownBug, DISABLED_Kstest2PValue)
{
    eval("[h,p,k]=kstest2([1 2 3 4 5],[2 3 4 5 6 7]);");
    EXPECT_NEAR(evalScalar("k"), 0.3333333, 1e-6);   // statistic already OK
    EXPECT_NEAR(evalScalar("p"), 0.8470543, 1e-5);   // numkit currently 0.9223
}

// bugs/stats/dwtest-pvalue.md — DW statistic correct, p-value method differs.
TEST_F(StatsKnownBug, DISABLED_DwtestPValue)
{
    eval("[p,dw]=dwtest([1 2 1 3 2 4]', [ones(6,1) (1:6)']);");
    EXPECT_NEAR(evalScalar("dw"), 0.3142857, 1e-6);  // statistic already OK
    EXPECT_LT(evalScalar("p"), 1e-4);                // MATLAB ~0; numkit ~0.017
}

// bugs/stats/mahal-singular.md — mahal must handle a rank-deficient reference.
TEST_F(StatsKnownBug, DISABLED_MahalSingular)
{
    eval("d = mahal([1 1; 2 2], [0 0; 1 1; 2 2; 3 3]);");   // collinear X
    EXPECT_NEAR(evalScalar("d(1)"), 0.9505075, 1e-5);
}

// NOTE: combnk scalar-set bug FIXED — see libs/stats/tests/combnk_test.cpp.
