// libs/stats/tests/multcompare_test.cpp
//
// Regression guard for multcompare + the extended anova1 stats struct.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class MultcompareTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// anova1 now exposes means / n / s / gnames in the stats struct.
TEST_F(MultcompareTest, Anova1StatsHasFullFields)
{
    eval("g1 = randn(15, 1); g2 = 2 + randn(15, 1); g3 = -1 + randn(15, 1);"
         "y = [g1; g2; g3];"
         "grp = [ones(15, 1); 2 * ones(15, 1); 3 * ones(15, 1)];"
         "[~, ~, s] = anova1(y, grp);"
         "hasM = isfield(s, 'means'); hasN = isfield(s, 'n');"
         "hasS = isfield(s, 's');     hasG = isfield(s, 'gnames');");
    EXPECT_TRUE(evalScalar("hasM") > 0.5);
    EXPECT_TRUE(evalScalar("hasN") > 0.5);
    EXPECT_TRUE(evalScalar("hasS") > 0.5);
    EXPECT_TRUE(evalScalar("hasG") > 0.5);
}

// 3 groups with known well-separated means → multcompare correctly
// identifies which pair is significantly different.
TEST_F(MultcompareTest, ThreeGroupsDetectsSignificance)
{
    eval("g1 = 10 + 0.3 * randn(20, 1);"
         "g2 = 12 + 0.3 * randn(20, 1);"
         "g3 = 10 + 0.3 * randn(20, 1);"
         "y = [g1; g2; g3];"
         "grp = [ones(20, 1); 2 * ones(20, 1); 3 * ones(20, 1)];"
         "[~, ~, s] = anova1(y, grp);"
         "c = multcompare(s);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(c, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(c, 2)")), 6);
    // Row 1: (1, 2) — different means
    EXPECT_LT(evalScalar("c(1, 6)"), 0.01);
    // Row 2: (1, 3) — same mean
    EXPECT_GT(evalScalar("c(2, 6)"), 0.05);
    // Row 3: (2, 3) — different means
    EXPECT_LT(evalScalar("c(3, 6)"), 0.01);
}

// Mean differences in column 4 = means(i) - means(j).
TEST_F(MultcompareTest, MeanDifferenceColumnIsCorrect)
{
    eval("y = [10; 10; 11; 11; 14; 14];"
         "grp = [1; 1; 2; 2; 3; 3];"
         "[~, ~, s] = anova1(y, grp);"
         "c = multcompare(s);"
         "diff12 = c(1, 4); diff13 = c(2, 4); diff23 = c(3, 4);");
    EXPECT_NEAR(evalScalar("diff12"), -1.0, 1e-9);
    EXPECT_NEAR(evalScalar("diff13"), -4.0, 1e-9);
    EXPECT_NEAR(evalScalar("diff23"), -3.0, 1e-9);
}

// LSD method has no Bonferroni inflation so p-values are smaller.
TEST_F(MultcompareTest, LSDPValuesLowerThanBonferroni)
{
    eval("g1 = randn(15, 1); g2 = 0.5 + randn(15, 1); g3 = randn(15, 1);"
         "y = [g1; g2; g3];"
         "grp = [ones(15, 1); 2 * ones(15, 1); 3 * ones(15, 1)];"
         "[~, ~, s] = anova1(y, grp);"
         "c_b = multcompare(s, 0.05, 'bonferroni');"
         "c_l = multcompare(s, 0.05, 'lsd');"
         "any_lower = any(c_l(:, 6) <= c_b(:, 6));");
    EXPECT_TRUE(evalScalar("any_lower") > 0.5);
}

// CI contains 0 ⇔ p > alpha (sanity check).
TEST_F(MultcompareTest, CIContainsZeroIffNotSignificant)
{
    eval("g1 = randn(30, 1); g2 = 3 + randn(30, 1);"
         "y = [g1; g2];"
         "grp = [ones(30, 1); 2 * ones(30, 1)];"
         "[~, ~, s] = anova1(y, grp);"
         "c = multcompare(s);"
         "ci_excludes_0 = (c(1, 3) > 0) || (c(1, 5) < 0);");
    EXPECT_TRUE(evalScalar("ci_excludes_0") > 0.5);
    EXPECT_LT(evalScalar("c(1, 6)"), 0.05);
}

TEST_F(MultcompareTest, TukeyKramerNotSupportedThrows)
{
    EXPECT_THROW(eval("y = randn(20, 1); grp = ones(20, 1);"
                      "grp(11:end) = 2;"
                      "[~, ~, s] = anova1(y, grp);"
                      "multcompare(s, 0.05, 'tukey-kramer');"),
                 std::exception);
}
