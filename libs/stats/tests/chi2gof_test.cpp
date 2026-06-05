// libs/stats/tests/chi2gof_test.cpp
// chi2gof.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Chi2gofTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Path A: explicit Frequency + Expected (existing behavior, kept).
TEST_F(Chi2gofTest, ExplicitFrequencyExpected)
{
    eval("[h, p, st] = chi2gof((1:5)', 'Frequency', [10 12 8 14 6], "
         "'Expected', [10 10 10 10 10]);");
    EXPECT_DOUBLE_EQ(evalScalar("h"), 0.0);
    EXPECT_NEAR(evalScalar("p"), 0.4060058497, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("st.chi2stat"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("st.df"), 4.0);
    // gap closure: stats now has edges, O, E populated.
    EXPECT_DOUBLE_EQ(evalScalar("numel(st.O)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(st.E)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(st.edges)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("st.O(1)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("st.E(1)"), 10.0);
}

// 2026-05-08 — gap closure: auto-binning with NBins.
TEST_F(Chi2gofTest, AutoBinNBins)
{
    eval("x = (-3:0.05:3)';");
    eval("[h, p, st] = chi2gof(x, 'NBins', 6);");
    EXPECT_DOUBLE_EQ(evalScalar("h"), 0.0);
    EXPECT_NEAR(evalScalar("p"), 0.0933694889, 1e-9);
    EXPECT_NEAR(evalScalar("st.chi2stat"), 6.4078228, 1e-6);
    EXPECT_DOUBLE_EQ(evalScalar("st.df"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(st.edges)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("st.O(1)"), 21.0);
    EXPECT_DOUBLE_EQ(evalScalar("st.O(6)"), 20.0);
}

// gap closure: explicit Edges argument.
TEST_F(Chi2gofTest, ExplicitEdges)
{
    eval("x = (-3:0.05:3)';");
    eval("[h, p, st] = chi2gof(x, 'Edges', [-3 -1 0 1 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("h"), 1.0);
    EXPECT_NEAR(evalScalar("p"), 0.0248233873, 1e-9);
    EXPECT_NEAR(evalScalar("st.chi2stat"), 5.0361650, 1e-6);
    EXPECT_DOUBLE_EQ(evalScalar("st.df"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("st.O(1)"), 40.0);
    EXPECT_DOUBLE_EQ(evalScalar("st.O(4)"), 41.0);
    EXPECT_NEAR(evalScalar("st.E(1)"), 34.3957, 1e-3);
}

// gap closure: 'CDF' function-handle argument errors (deferred).
TEST_F(Chi2gofTest, CDFArgRejected)
{
    eval("x = (-3:0.05:3)';");
    bool threw = false;
    try {
        eval("chi2gof(x, 'CDF', 'normcdf');");
    } catch (const std::exception &) {
        threw = true;
    }
    EXPECT_TRUE(threw);
}

// 'EMin' merging: with high EMin, tail bins should merge.
TEST_F(Chi2gofTest, EMinMergesTailBins)
{
    eval("x = (-3:0.05:3)';");
    eval("[h, p, st] = chi2gof(x, 'NBins', 20, 'EMin', 12);");
    // With EMin=12, the tails (where E is small) should be merged into
    // larger central bins → fewer than 20 final bins.
    EXPECT_LT(evalScalar("numel(st.O)"), 20.0);
}
