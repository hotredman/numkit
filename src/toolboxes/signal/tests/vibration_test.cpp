// toolboxes/signal/tests/vibration_test.cpp
//
// Coverage for measurements/vibration.cpp (rainflow) — previously gtest-
// uncovered. rainflow does fatigue cycle counting; for the load history
// [0 1 -1 2 -2 0] it returns a 5-row cycle-count matrix whose first row is a
// half-cycle (count 0.5) of range 1.

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class VibrationTest : public DualEngineTest
{};

TEST_P(VibrationTest, RainflowCycleCount)
{
    eval("c = rainflow([0 1 -1 2 -2 0]);");
    EXPECT_EQ(eval("c").dims().rows(), 5u);   // 5 counted cycles
    EXPECT_EQ(eval("c").dims().cols(), 5u);   // [count range mean start period]
    EXPECT_NEAR(evalScalar("c(1,1)"), 0.5, 1e-12);  // half cycle
    EXPECT_NEAR(evalScalar("c(1,2)"), 1.0, 1e-12);  // range
}

TEST_P(VibrationTest, RainflowCountsAreHalfOrFull)
{
    // Every cycle count is 0.5 (half) or 1.0 (full).
    eval("c = rainflow([0 2 1 3 -1 1 -2 0]); allcnt = c(:,1); "
         "bad = sum((abs(allcnt - 0.5) > 1e-9) & (abs(allcnt - 1.0) > 1e-9));");
    EXPECT_DOUBLE_EQ(evalScalar("bad"), 0.0);
}

INSTANTIATE_DUAL(VibrationTest);
