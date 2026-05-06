// libs/stats/tests/normlike_upgrade_test.cpp
//
// Backfill gtest for the normlike censoring + freq upgrade shipped
// in commit bb17c91. Reference values from MATLAB R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class NormlikeUpgradeTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]';");
        engine.eval("cens = [0 0 0 0 0 1 1]';");
        engine.eval("freq = [2 2 2 1 1 1 1]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(NormlikeUpgradeTest, BasicNoExtras)
{
    EXPECT_NEAR(evalScalar("normlike([3, 1.5], x)"), 17.4730477, 1e-5);
}

TEST_F(NormlikeUpgradeTest, WithCensoring)
{
    EXPECT_NEAR(evalScalar("normlike([3, 1.5], x, cens)"), 18.6858148, 1e-5);
}

TEST_F(NormlikeUpgradeTest, WithFreq)
{
    EXPECT_NEAR(evalScalar("normlike([3, 1.5], x, [], freq)"), 22.2484809, 1e-5);
}

TEST_F(NormlikeUpgradeTest, CensoringPlusFreq)
{
    EXPECT_NEAR(evalScalar("normlike([3, 1.5], x, cens, freq)"), 23.4612480, 1e-5);
}

TEST_F(NormlikeUpgradeTest, EmptyDataReturnsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("normlike([3, 1.5], [])"), 0.0);
}

TEST_F(NormlikeUpgradeTest, SigmaZeroReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("normlike([3, 0], x)")));
}

TEST_F(NormlikeUpgradeTest, NegativeSigmaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("normlike([3, -1], x)")));
}

TEST_F(NormlikeUpgradeTest, NaNInDataReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("normlike([3, 1.5], [1 2 NaN 4]')")));
}

TEST_F(NormlikeUpgradeTest, ZeroFreqDropsElement)
{
    eval("y1 = normlike([3, 1.5], x, [], [1 1 1 1 1 1 0]');");
    eval("y2 = normlike([3, 1.5], x(1:6));");
    EXPECT_DOUBLE_EQ(evalScalar("y1"), evalScalar("y2"));
}

// ── aVar (2nd output: inverse observed-Fisher 2×2) ──────────────────

TEST_F(NormlikeUpgradeTest, AVarBasic)
{
    eval("[nL, av] = normlike([3, 1.5], x);");
    EXPECT_NEAR(evalScalar("av(1,1)"),  0.5685760656, 1e-9);
    EXPECT_NEAR(evalScalar("av(1,2)"), -0.1526499228, 1e-9);
    EXPECT_NEAR(evalScalar("av(2,1)"), -0.1526499228, 1e-9);  // symmetry
    EXPECT_NEAR(evalScalar("av(2,2)"),  0.0942837759, 1e-9);
}

TEST_F(NormlikeUpgradeTest, AVarWithCensoring)
{
    eval("[nL, av] = normlike([3, 1.5], x, cens);");
    EXPECT_NEAR(evalScalar("av(1,1)"),  0.5719686586, 1e-9);
    EXPECT_NEAR(evalScalar("av(1,2)"), -0.1426189548, 1e-9);
    EXPECT_NEAR(evalScalar("av(2,2)"),  0.0841378833, 1e-9);
}

TEST_F(NormlikeUpgradeTest, AVarWithFreq)
{
    eval("[nL, av] = normlike([3, 1.5], x, [], freq);");
    EXPECT_NEAR(evalScalar("av(1,1)"),  0.2663412361, 1e-9);
    EXPECT_NEAR(evalScalar("av(1,2)"), -0.0500095598, 1e-9);
    EXPECT_NEAR(evalScalar("av(2,2)"),  0.0604954352, 1e-9);
}

TEST_F(NormlikeUpgradeTest, AVarCensoringPlusFreq)
{
    eval("[nL, av] = normlike([3, 1.5], x, cens, freq);");
    EXPECT_NEAR(evalScalar("av(1,1)"),  0.2704780402, 1e-9);
    EXPECT_NEAR(evalScalar("av(1,2)"), -0.0476691396, 1e-9);
    EXPECT_NEAR(evalScalar("av(2,2)"),  0.0551473719, 1e-9);
}
