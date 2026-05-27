// libs/stats/tests/filloutliers_test.cpp
//
// Regression guard for filloutliers — outlier detection + replacement.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FillOutliersTest : public ::testing::Test
{
public:
    Engine engine;
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
