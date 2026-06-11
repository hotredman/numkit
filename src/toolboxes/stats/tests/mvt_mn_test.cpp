// toolboxes/stats/tests/mvt_mn_test.cpp
//
// Regression guard for mvtrnd + mnrnd.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class MvtMnTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── mvtrnd ──────────────────────────────────────────────────────────

TEST_F(MvtMnTest, MvtrndShapeIsNByD)
{
    eval("R = mvtrnd(eye(3), 4, 100);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 1)")), 100);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 2)")), 3);
}

// For df > 2 the t-cov is df/(df-2) · C. Sample cov * (df-2)/df ≈ C.
TEST_F(MvtMnTest, MvtrndCovarianceMatchesScaledC)
{
    eval("C = [1 0.5; 0.5 1]; df = 5;"
         "R = mvtrnd(C, df, 5000);"
         "Cs = cov(R) * (df - 2) / df;"
         "err = max(abs(Cs(:) - C(:)));");
    EXPECT_LT(evalScalar("err"), 0.15);
}

TEST_F(MvtMnTest, MvtrndZeroDfThrows)
{
    EXPECT_THROW(eval("mvtrnd(eye(2), 0, 10);"), std::exception);
}

// ── mnrnd ───────────────────────────────────────────────────────────

TEST_F(MvtMnTest, MnrndRowSumsToN)
{
    eval("M = mnrnd(50, [0.2, 0.3, 0.5], 100);"
         "rows = sum(M, 2);"
         "all_50 = all(rows == 50);");
    EXPECT_TRUE(evalScalar("all_50") > 0.5);
}

TEST_F(MvtMnTest, MnrndApproachesExpectedColMeans)
{
    eval("p = [0.2, 0.3, 0.5]; N = 100;"
         "M = mnrnd(N, p, 1000); m = mean(M);");
    EXPECT_NEAR(evalScalar("m(1)"), 20.0, 1.5);
    EXPECT_NEAR(evalScalar("m(2)"), 30.0, 1.5);
    EXPECT_NEAR(evalScalar("m(3)"), 50.0, 2.0);
}

TEST_F(MvtMnTest, MnrndRenormalisesProbabilities)
{
    // P does not sum to 1 — should still work after renormalisation.
    eval("M = mnrnd(100, [1, 2, 3], 200);"
         "rows = sum(M, 2);"
         "all_100 = all(rows == 100);");
    EXPECT_TRUE(evalScalar("all_100") > 0.5);
}

TEST_F(MvtMnTest, MnrndNegativeProbThrows)
{
    EXPECT_THROW(eval("mnrnd(10, [0.5, -0.3, 0.8]);"), std::exception);
}

TEST_F(MvtMnTest, MnrndSingleSampleDefault)
{
    eval("r = mnrnd(20, [0.3, 0.7]);"
         "ok = size(r, 1) == 1 && size(r, 2) == 2;");
    EXPECT_TRUE(evalScalar("ok") > 0.5);
}
