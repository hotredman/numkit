// libs/stats/tests/ttest_extras_test.cpp
//
// Closes audit/closed/stats/{ttest,ttest2}.md.
// Reference values from MATLAB R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TtestExtrasTest : public ::testing::Test
{
public:
    Engine engine;
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
    eval("[h, p, ci, t] = ttest(x, y);");
    // x - y = constant 0.4 + 0.5 + 0.4 + 0.5 + 0.5; tstat is large positive.
    EXPECT_GT(evalScalar("t"), 10.0);
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
    EXPECT_DOUBLE_EQ(evalScalar("te"), evalScalar("td"));
}

TEST_F(TtestExtrasTest, Ttest2EqualValueMatchesMATLAB)
{
    EXPECT_NEAR(evalScalar("[~,~,~,t] = ttest2(x, y); t"), 0.475466, 1e-5);
}

TEST_F(TtestExtrasTest, Ttest2DimRejected)
{
    EXPECT_THROW(eval("ttest2(x, y, 'Dim', 1);"), numkit::Error);
}
