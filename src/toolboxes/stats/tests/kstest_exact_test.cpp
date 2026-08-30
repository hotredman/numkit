// toolboxes/stats/tests/kstest_exact_test.cpp
//
// Regression guard for bugs/stats/kstest-pvalue.md (FIXED): kstest/kstest2 now
// use the EXACT finite-n Kolmogorov distribution (MATLAB default) — two-sided
// via Marsaglia-Tsang-Wang, one-sided via Birnbaum-Tingey — with critical
// values by inverting the same p-function, and kstest2 via Stephens' corrected
// asymptotic. The KS statistic was already correct. MATLAB R2025b reference.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class KstestExactTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Two-sided exact p + critical value (the repro).
TEST_F(KstestExactTest, OneSampleTwoSided)
{
    eval("[h,p,ks,cv] = kstest([-1 0 1 2 -0.5 0.5]);");
    EXPECT_NEAR(evalScalar("ks"), 0.19146246, 1e-7);
    EXPECT_NEAR(evalScalar("p"),  0.949984105, 1e-7);   // exact (was 0.9804)
    EXPECT_NEAR(evalScalar("cv"), 0.51926, 1e-4);       // exact (was 0.5544)
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 0);
}

// Critical value at several alphas matches the exact KS table (n=6).
TEST_F(KstestExactTest, CriticalValuesByAlpha)
{
    eval("x = [-1 0 1 2 -0.5 0.5];");
    eval("[~,~,~,c10] = kstest(x, 'Alpha', 0.10);");
    eval("[~,~,~,c05] = kstest(x, 'Alpha', 0.05);");
    eval("[~,~,~,c01] = kstest(x, 'Alpha', 0.01);");
    EXPECT_NEAR(evalScalar("c10"), 0.46799, 1e-4);
    EXPECT_NEAR(evalScalar("c05"), 0.51926, 1e-4);
    EXPECT_NEAR(evalScalar("c01"), 0.61661, 1e-4);
}

// One-sided exact (Birnbaum-Tingey).
TEST_F(KstestExactTest, OneSampleOneSided)
{
    eval("x = [-1 0 1 2 -0.5 0.5];");
    eval("[~,pl,~,cvl] = kstest(x, 'Tail', 'larger');");
    eval("[~,ps] = kstest(x, 'Tail', 'smaller');");
    EXPECT_NEAR(evalScalar("pl"),  0.9719737686, 1e-7);
    EXPECT_NEAR(evalScalar("ps"),  0.5717052286, 1e-7);
    EXPECT_NEAR(evalScalar("cvl"), 0.46799, 1e-4);   // one-sided cv (alpha=0.05)
}

// A second design (n=12) in the exact regime.
TEST_F(KstestExactTest, N12)
{
    eval("[h,p,ks,cv] = kstest([0.1 0.3 -0.2 1.5 -1.1 0.6 -0.4 0.9 -0.8 0.2 -1.7 1.2]);");
    EXPECT_NEAR(evalScalar("ks"), 0.12316117, 1e-7);
    EXPECT_NEAR(evalScalar("p"),  0.9828272287, 1e-7);
    EXPECT_NEAR(evalScalar("cv"), 0.37543, 1e-4);
}

// Far-shifted data → tiny exact p (near the FP floor).
TEST_F(KstestExactTest, TinyP)
{
    eval("[h,p,ks] = kstest([3 4 5 6 7]);");
    EXPECT_NEAR(evalScalar("ks"), 0.9986501, 1e-6);
    EXPECT_LT(evalScalar("p"), 1e-13);
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 1);
}

// kstest2: Stephens' corrected asymptotic (two-sided + one-sided).
TEST_F(KstestExactTest, TwoSample)
{
    eval("[h,p,ks] = kstest2([1 2 3 4 5], [2 3 4 5 6 7]);");
    EXPECT_NEAR(evalScalar("ks"), 0.33333333, 1e-7);
    EXPECT_NEAR(evalScalar("p"),  0.847054342, 1e-7);   // was 0.9223
    eval("[~,plg] = kstest2([1 2 3 4 5], [2 3 4 5 6 7], 'Tail', 'larger');");
    eval("[~,psm] = kstest2([1 2 3 4 5], [2 3 4 5 6 7], 'Tail', 'smaller');");
    EXPECT_NEAR(evalScalar("plg"), 0.472005347, 1e-7);
    EXPECT_DOUBLE_EQ(evalScalar("psm"), 1.0);
}
