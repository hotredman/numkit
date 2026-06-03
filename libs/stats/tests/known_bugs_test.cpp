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

// bugs/stats/anova1-matrix-input.md — column-per-group matrix form.
TEST_F(StatsKnownBug, DISABLED_Anova1MatrixInput)
{
    eval("p = anova1([1 2 3; 2 3 4; 3 4 5]);");
    EXPECT_NEAR(evalScalar("p"), 0.125000, 1e-5);
}

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
