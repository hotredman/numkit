// toolboxes/stats/tests/filloutliers_test.cpp
//
// Regression guard for filloutliers — outlier detection + replacement.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FillOutliersTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("x = [1 2 3 4 100 5 6 7];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── fill methods ─────────────────────────────────────────────────

TEST_F(FillOutliersTest, LinearFill)
{
    eval("B = filloutliers(x, 'linear');");
    EXPECT_NEAR(evalScalar("B(5)"), 4.5, 1e-12);
    EXPECT_NEAR(evalScalar("B(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(8)"), 7.0, 1e-12);
}

TEST_F(FillOutliersTest, PreviousFill)
{
    eval("B = filloutliers(x, 'previous');");
    EXPECT_NEAR(evalScalar("B(5)"), 4.0, 1e-12);
}

TEST_F(FillOutliersTest, NextFill)
{
    eval("B = filloutliers(x, 'next');");
    EXPECT_NEAR(evalScalar("B(5)"), 5.0, 1e-12);
}

TEST_F(FillOutliersTest, NearestFillTieToNext)
{
    eval("B = filloutliers(x, 'nearest');");
    EXPECT_NEAR(evalScalar("B(5)"), 5.0, 1e-12);
}

TEST_F(FillOutliersTest, CenterFill)
{
    eval("B = filloutliers(x, 'center');");
    EXPECT_NEAR(evalScalar("B(5)"), 4.5, 1e-12);
}

TEST_F(FillOutliersTest, ClipFill)
{
    // Threshold for median+MAD: med=4.5, scaled_MAD=2.0*1.4826=2.9652,
    // U = 4.5 + 3*2.9652 = 13.3956. 100 clipped to U.
    eval("B = filloutliers(x, 'clip');");
    EXPECT_NEAR(evalScalar("B(5)"), 13.3956133110336, 1e-9);
}

TEST_F(FillOutliersTest, ConstantFill)
{
    eval("B = filloutliers(x, -99);");
    EXPECT_NEAR(evalScalar("B(5)"), -99.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(1)"),   1.0, 1e-12);
}

// ── detection methods ─────────────────────────────────────────────

TEST_F(FillOutliersTest, MeanDetectionDoesNotFlag100AtDefaultTF)
{
    // mean+3std doesn't flag 100 at default tf=3 in this 8-sample vector
    // (probed against MATLAB R2025b).
    eval("B = filloutliers(x, 'linear', 'mean');");
    EXPECT_NEAR(evalScalar("B(5)"), 100.0, 1e-12);
}

TEST_F(FillOutliersTest, MeanDetectionFlagsAtTighterTF)
{
    eval("B = filloutliers(x, 'linear', 'mean', 'ThresholdFactor', 1);");
    EXPECT_NEAR(evalScalar("B(5)"), 4.5, 1e-12);
}

TEST_F(FillOutliersTest, QuartilesDetection)
{
    eval("B = filloutliers(x, 'linear', 'quartiles');");
    EXPECT_NEAR(evalScalar("B(5)"), 4.5, 1e-12);
}

// ── errors ───────────────────────────────────────────────────────

TEST_F(FillOutliersTest, NoArgsThrows)
{
    EXPECT_THROW(eval("filloutliers();"), std::exception);
}

TEST_F(FillOutliersTest, BadFillMethodThrows)
{
    EXPECT_THROW(eval("filloutliers(x, 'gibberish');"), std::exception);
}

TEST_F(FillOutliersTest, BadFindMethodThrows)
{
    EXPECT_THROW(eval("filloutliers(x, 'linear', 'gibberish');"), std::exception);
}

// ── 'percentiles' detection method (DEEP-PROBE 2026-05-31) ───────
// filloutliers(A, fillmethod, 'percentiles', [lo hi]) previously threw
// 'findmethod must be median/mean/quartiles'. Bounds use MATLAB prctile;
// 'center' fill = midpoint (lo+hi)/2; 'clip' clips to [lo,hi]. vs MATLAB
// R2025b on xp = [1 2 3 100 4 5].
TEST_F(FillOutliersTest, PercentilesClip)
{
    eval("cp = filloutliers([1 2 3 100 4 5], 'clip', 'percentiles', [10 90]);");
    EXPECT_NEAR(evalScalar("cp(1)"), 1.1, 1e-12);
    EXPECT_NEAR(evalScalar("cp(4)"), 90.5, 1e-10);
    EXPECT_NEAR(evalScalar("cp(2)"), 2.0, 1e-12);   // non-outlier unchanged
}

TEST_F(FillOutliersTest, PercentilesCenterIsMidpoint)
{
    eval("ce = filloutliers([1 2 3 100 4 5], 'center', 'percentiles', [10 90]);");
    EXPECT_NEAR(evalScalar("ce(1)"), 45.8, 1e-10);  // (1.1 + 90.5)/2
    EXPECT_NEAR(evalScalar("ce(4)"), 45.8, 1e-10);
}

TEST_F(FillOutliersTest, PercentilesConstantFill)
{
    eval("c0 = filloutliers([1 2 3 100 4 5], 0, 'percentiles', [10 90]);");
    EXPECT_DOUBLE_EQ(evalScalar("c0(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("c0(4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("c0(2)"), 2.0);
}

TEST_F(FillOutliersTest, PercentilesClip2575)
{
    eval("c = filloutliers([1 2 3 100 4 5], 'clip', 'percentiles', [25 75]);");
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 2.0);   // 1 -> prctile 25 = 2
    EXPECT_DOUBLE_EQ(evalScalar("c(4)"), 5.0);   // 100 -> prctile 75 = 5
}

TEST_F(FillOutliersTest, PercentilesMatrixPerColumn)
{
    eval("R = filloutliers([1 2; 3 4; 5 100; 7 8; 9 10], 'clip', 'percentiles', [20 80]);");
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"), 2.0);   // col1: 1 -> 2
    EXPECT_DOUBLE_EQ(evalScalar("R(5,1)"), 8.0);   // col1: 9 -> 8
    EXPECT_DOUBLE_EQ(evalScalar("R(3,2)"), 55.0);  // col2: 100 -> 55
    EXPECT_DOUBLE_EQ(evalScalar("R(1,2)"), 3.0);   // col2: 2 -> 3
}

TEST_F(FillOutliersTest, PercentilesNeedsTwoElementVector)
{
    EXPECT_THROW(eval("filloutliers([1 2 3], 'clip', 'percentiles', 50);"), std::exception);
}
