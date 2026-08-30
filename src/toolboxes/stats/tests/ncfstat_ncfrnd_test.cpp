// toolboxes/stats/tests/ncfstat_ncfrnd_test.cpp
//
// Regression guard for ncfstat + ncfrnd.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class NcfStatRndTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── ncfstat ─────────────────────────────────────────────────────────

TEST_F(NcfStatRndTest, NcfstatMatchesMatlab)
{
    eval("[m, v] = ncfstat(5, 10, 3);");
    EXPECT_NEAR(evalScalar("m"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("v"), 3.1666666667, 1e-9);
}

TEST_F(NcfStatRndTest, NcfstatLargeNu)
{
    eval("[m, v] = ncfstat(8, 20, 2);");
    EXPECT_NEAR(evalScalar("m"), 1.3888888889, 1e-9);
    EXPECT_NEAR(evalScalar("v"), 0.7619598765, 1e-9);
}

TEST_F(NcfStatRndTest, NcfstatDeltaZeroEqualsCentral)
{
    eval("[mn, vn] = ncfstat(5, 10, 0); [mc, vc] = fstat(5, 10);");
    EXPECT_NEAR(evalScalar("mn"), evalScalar("mc"), 1e-12);
    EXPECT_NEAR(evalScalar("vn"), evalScalar("vc"), 1e-12);
}

TEST_F(NcfStatRndTest, NcfstatNu2EqualsTwoMeanNan)
{
    eval("[m, v] = ncfstat(5, 2, 3);");
    EXPECT_TRUE(std::isnan(evalScalar("m")));
    EXPECT_TRUE(std::isnan(evalScalar("v")));
}

TEST_F(NcfStatRndTest, NcfstatNu2EqualsFourVarNan)
{
    eval("[m, v] = ncfstat(5, 4, 3);");
    EXPECT_FALSE(std::isnan(evalScalar("m")));
    EXPECT_TRUE(std::isnan(evalScalar("v")));
}

// ── ncfrnd ──────────────────────────────────────────────────────────

TEST_F(NcfStatRndTest, NcfrndShape)
{
    eval("R = ncfrnd(5, 10, 3, 7, 3);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 1)")), 7);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 2)")), 3);
}

TEST_F(NcfStatRndTest, NcfrndMeanMatches)
{
    eval("S = ncfrnd(5, 10, 3, 5000, 1); m = mean(S);");
    EXPECT_NEAR(evalScalar("m"), 2.0, 0.2);
}

TEST_F(NcfStatRndTest, NcfrndAllPositive)
{
    eval("S = ncfrnd(5, 10, 3, 1000, 1); mn = min(S);");
    EXPECT_GT(evalScalar("mn"), 0.0);
}

TEST_F(NcfStatRndTest, NcfrndDeltaZeroIsCentralF)
{
    // δ = 0 ⇒ central F with mean ν₂/(ν₂-2). For ν₂=10 mean=1.25.
    eval("S = ncfrnd(5, 10, 0, 5000, 1); m = mean(S);");
    EXPECT_NEAR(evalScalar("m"), 1.25, 0.15);
}
