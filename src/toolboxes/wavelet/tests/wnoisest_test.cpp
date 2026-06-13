// toolboxes/wavelet/tests/wnoisest_test.cpp
//
// Coverage for wnoisest(C, L, S) — per-level noise sigma estimate from a
// wavedec decomposition. MATLAB uses the robust MAD estimator on the detail
// coefficients of each requested level: sigma = median(|d|) / 0.6745.
// Checked both against the exact value and for self-consistency with detcoef.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class WnoisestTest : public DualEngineTest
{};

TEST_P(WnoisestTest, MadEstimatePerLevel)
{
    eval("x = [1 2 3 4 5 6 7 8 9 10 9 8 7 6 5 4];");
    eval("[C, L] = wavedec(x, 2, 'db1');");
    // Level-1 estimate: exact value + equals the MAD of the level-1 details.
    EXPECT_NEAR(evalScalar("wnoisest(C, L, 1)"), 1.0483421515, 1e-9);
    EXPECT_NEAR(evalScalar("wnoisest(C, L, 1)"),
                evalScalar("median(abs(detcoef(C, L, 1))) / 0.6745"), 1e-12);
    // Vector of levels returns one estimate per requested level.
    eval("s = wnoisest(C, L, [1 2]);");
    EXPECT_EQ(eval("s").numel(), 2u);
    EXPECT_NEAR(evalScalar("s(1)"), 1.0483421515, 1e-9);
    EXPECT_NEAR(evalScalar("s(2)"), 2.9651593773, 1e-9);
}

INSTANTIATE_DUAL(WnoisestTest);
