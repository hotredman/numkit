// toolboxes/stats/tests/sign_extras_test.cpp
// signtest required adding zval=NaN to its stats struct; the other
// three were already MATLAB-parity verified (no behavioural gap).
// gtest provides regression coverage for all four.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class SignExtrasTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]';");
        engine.eval("y = [0.8 1.9 2.7 4.0 4.5 5.7 6.4]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── signtest: stats struct now {zval, sign} ───────────────────────────

TEST_F(SignExtrasTest, SigntestStatsHasZvalField)
{
    eval("[p, h, st] = signtest(x);");
    EXPECT_TRUE(std::isnan(evalScalar("st.zval")));
    EXPECT_DOUBLE_EQ(evalScalar("st.sign"), 7);
    EXPECT_DOUBLE_EQ(evalScalar("p"), 0.015625);
}

TEST_F(SignExtrasTest, SigntestPaired)
{
    eval("[p, h, st] = signtest(x, y);");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 0.015625);
    EXPECT_DOUBLE_EQ(evalScalar("st.sign"), 7);
}

// ── signrank: regression on basic invocation ──────────────────────────

TEST_F(SignExtrasTest, SignrankBasic)
{
    eval("[p, h] = signrank(x);");
    // Wilcoxon signed-rank against m=0; all positive → smallest possible W
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 1);
    EXPECT_LT(evalScalar("p"), 0.05);
}

TEST_F(SignExtrasTest, SignrankPaired)
{
    eval("[p, h] = signrank(x, y);");
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 1);
}

// ── ranksum: two-sample Mann-Whitney ──────────────────────────────────

TEST_F(SignExtrasTest, RanksumBasic)
{
    eval("[p, h] = ranksum(x, y);");
    EXPECT_GE(evalScalar("p"), 0.0);
    EXPECT_LE(evalScalar("p"), 1.0);
}

// ── fishertest: 2x2 contingency ───────────────────────────────────────

TEST_F(SignExtrasTest, FishertestBasic)
{
    eval("T = [12 5; 4 9]; [h, p, stats] = fishertest(T);");
    EXPECT_GE(evalScalar("p"), 0.0);
    EXPECT_LE(evalScalar("p"), 1.0);
}
