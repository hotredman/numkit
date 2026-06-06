// libs/stats/tests/ttest_extras_test.cpp
// Reference values from MATLAB R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TtestExtrasTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1.2 2.4 3.1 4.5 5.0]';");
        engine.eval("y = [0.8 1.9 2.7 4.0 4.5]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── ttest paired form ─────────────────────────────────────────────────

TEST_F(TtestExtrasTest, PairedFormDoesNotThrow)
{
    EXPECT_NO_THROW(eval("ttest(x, y);"));
}

TEST_F(TtestExtrasTest, PairedFormProducesValidTstat)
{
    // 4th output is the MATLAB stats struct {tstat, df, sd}.
    eval("[h, p, ci, st] = ttest(x, y);");
    EXPECT_GT(evalScalar("st.tstat"), 10.0);
    EXPECT_EQ(static_cast<int>(evalScalar("st.df")), 4);   // n-1 = 4
    EXPECT_GT(evalScalar("st.sd"), 0.0);
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 1);
}

// ── ttest Name-Value Alpha ────────────────────────────────────────────

TEST_F(TtestExtrasTest, AlphaNVChangesCI)
{
    // 99% CI vs 95% CI must differ in width.
    eval("[~,~,ci95] = ttest(x, 4, 'Alpha', 0.05); [~,~,ci99] = ttest(x, 4, 'Alpha', 0.01);");
    EXPECT_GT(evalScalar("(ci99(2) - ci99(1))"),
              evalScalar("(ci95(2) - ci95(1))"));
}

TEST_F(TtestExtrasTest, TailNVRecognised)
{
    EXPECT_NO_THROW(eval("ttest(x, 4, 'Tail', 'right');"));
    EXPECT_NO_THROW(eval("ttest(x, 4, 'Tail', 'left');"));
}

// ── ttest2 default = equal (pooled) ────────────────────────────────────

TEST_F(TtestExtrasTest, Ttest2DefaultIsEqualPooled)
{
    eval("[~,~,~,te] = ttest2(x, y); [~,~,~,td] = ttest2(x, y, 'Vartype', 'equal');");
    EXPECT_DOUBLE_EQ(evalScalar("te.tstat"), evalScalar("td.tstat"));
}

TEST_F(TtestExtrasTest, Ttest2EqualValueMatchesMATLAB)
{
    eval("[~,~,~,st] = ttest2(x, y);");
    EXPECT_NEAR(evalScalar("st.tstat"), 0.475466, 1e-5);
}

// The 4th output of the parametric tests is MATLAB's stats struct. vs R2025b.
// ttest/ttest2 -> {tstat,df,sd}; vartest -> {chisqstat,df};
// vartest2 -> {fstat,df1,df2}; ztest -> bare zval scalar (unchanged).
TEST_F(TtestExtrasTest, ParametricStatsStructs)
{
    eval("a = 1:10; b = [2 3 4 5 6 8 9 11 13 15];");
    eval("[~,~,~,s1] = ttest(a, 5);");
    EXPECT_NEAR(evalScalar("s1.tstat"), 0.5222328, 1e-6);
    EXPECT_DOUBLE_EQ(evalScalar("s1.df"), 9.0);
    EXPECT_NEAR(evalScalar("s1.sd"), 3.0276504, 1e-6);
    eval("[~,~,~,s2] = ttest2(a, b);");        // default 'equal' -> pooled sd
    EXPECT_NEAR(evalScalar("s2.tstat"), -1.2478312, 1e-6);
    EXPECT_DOUBLE_EQ(evalScalar("s2.df"), 18.0);
    EXPECT_NEAR(evalScalar("s2.sd"), 3.7631250, 1e-6);
    eval("[~,~,~,s4] = vartest(a, 5);");
    EXPECT_NEAR(evalScalar("s4.chisqstat"), 16.5, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("s4.df"), 9.0);
    eval("[~,~,~,s5] = vartest2(a, b);");
    EXPECT_NEAR(evalScalar("s5.fstat"), 0.4785384, 1e-6);
    EXPECT_DOUBLE_EQ(evalScalar("s5.df1"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("s5.df2"), 9.0);
    // ztest's 4th output stays a bare scalar (zval), not a struct.
    eval("[~,~,~,zv] = ztest(a, 5, 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("isstruct(zv)")), 0);
    EXPECT_NEAR(evalScalar("zv"), 0.7905694, 1e-6);
}

TEST_F(TtestExtrasTest, Ttest2DimRejected)
{
    EXPECT_THROW(eval("ttest2(x, y, 'Dim', 1);"), numkit::Error);
}
