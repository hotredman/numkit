// toolboxes/stats/tests/fillmissing_test.cpp
//
// Regression guard for fillmissing methods — extended to cover
// 'nearest' and 'linear' (added cycle 74) plus per-column matrix
// processing for the previously-flat 'previous'/'next'/'constant'
// methods.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class FillMissingTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── nearest ──────────────────────────────────────────────────────

TEST_F(FillMissingTest, NearestBasicTieToNext)
{
    // Middle NaN ties at distance 1 to both 4 and 6 → MATLAB picks NEXT (6).
    eval("B = fillmissing([1 NaN NaN 4 NaN 6], 'nearest');");
    EXPECT_NEAR(evalScalar("B(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(4)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(5)"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(6)"), 6.0, 1e-12);
}

TEST_F(FillMissingTest, NearestLeadingTrailingNaNs)
{
    eval("B = fillmissing([NaN NaN 3 NaN 5 NaN NaN], 'nearest');");
    EXPECT_NEAR(evalScalar("B(1)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(4)"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(5)"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(6)"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(7)"), 5.0, 1e-12);
}

TEST_F(FillMissingTest, NearestAllNaNNoOp)
{
    eval("B = fillmissing([NaN NaN], 'nearest');");
    EXPECT_TRUE(std::isnan(evalScalar("B(1)")));
    EXPECT_TRUE(std::isnan(evalScalar("B(2)")));
}

// ── linear ───────────────────────────────────────────────────────

TEST_F(FillMissingTest, LinearInteriorOnly)
{
    eval("B = fillmissing([1 NaN NaN 4 NaN 6], 'linear');");
    for (int i = 1; i <= 6; ++i)
        EXPECT_NEAR(evalScalar("B(" + std::to_string(i) + ")"), double(i), 1e-12);
}

TEST_F(FillMissingTest, LinearExtrapolatesEnds)
{
    // Slopes from interior pairs (3,5) and (3,5) extrapolate to
    // leading [1,2] and trailing [6,7].
    eval("B = fillmissing([NaN NaN 3 NaN 5 NaN NaN], 'linear');");
    for (int i = 1; i <= 7; ++i)
        EXPECT_NEAR(evalScalar("B(" + std::to_string(i) + ")"), double(i), 1e-12);
}

TEST_F(FillMissingTest, LinearLeadingAndTrailingNaN)
{
    eval("B = fillmissing([NaN 1 2 NaN 4 NaN], 'linear');");
    for (int i = 0; i <= 5; ++i)
        EXPECT_NEAR(evalScalar("B(" + std::to_string(i+1) + ")"), double(i), 1e-12);
}

// ── matrix per-column ────────────────────────────────────────────

TEST_F(FillMissingTest, MatrixLinearPerColumn)
{
    eval("B = fillmissing([1 NaN; NaN 4; 3 NaN; 5 8], 'linear');");
    // col 1: 1, 2 (interp), 3, 5
    EXPECT_NEAR(evalScalar("B(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,1)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,1)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(4,1)"), 5.0, 1e-12);
    // col 2: 2 (extrap), 4, 6 (interp), 8
    EXPECT_NEAR(evalScalar("B(1,2)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,2)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,2)"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(4,2)"), 8.0, 1e-12);
}

TEST_F(FillMissingTest, MatrixPreviousPerColumn)
{
    // Per-column previous: each column is independent. Col 2 first
    // element stays NaN because no good value precedes it.
    eval("B = fillmissing([1 NaN; NaN 4; 3 NaN; 5 8], 'previous');");
    EXPECT_NEAR(evalScalar("B(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,1)"), 1.0, 1e-12);   // carry from above
    EXPECT_TRUE(std::isnan(evalScalar("B(1,2)")));   // NOT carried from col1 end
    EXPECT_NEAR(evalScalar("B(2,2)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,2)"), 4.0, 1e-12);
}

// ── existing methods still work ──────────────────────────────────

TEST_F(FillMissingTest, ConstantStillWorks)
{
    eval("B = fillmissing([1 NaN 3 NaN], 'constant', -99);");
    EXPECT_NEAR(evalScalar("B(1)"),   1.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), -99.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3)"),   3.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(4)"), -99.0, 1e-12);
}

TEST_F(FillMissingTest, NextStillWorks)
{
    eval("B = fillmissing([1 NaN NaN 4], 'next');");
    EXPECT_NEAR(evalScalar("B(2)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3)"), 4.0, 1e-12);
}

// ── errors ───────────────────────────────────────────────────────

TEST_F(FillMissingTest, UnknownMethodThrows)
{
    EXPECT_THROW(eval("fillmissing([1 NaN 3], 'wat');"), std::exception);
}
