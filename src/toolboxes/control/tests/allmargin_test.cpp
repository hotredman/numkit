// toolboxes/control/tests/allmargin_test.cpp
//
// allmargin(sys) — all gain/phase/delay margins + closed-loop stability as
// a 7-field struct. bugs/control/allmargin.md. Reference values from
// MATLAB R2025b. The crossovers are computed exactly (G(jω) scan +
// bisection), so margins match MATLAB to ~6 digits.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class AllmarginTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// 1/((s+1)(s+2)(s+3)): one gain margin (GM=60 at ω=sqrt(11)), no gain
// crossover (|G|<1 ∀ω), closed loop stable.
TEST_F(AllmarginTest, ThirdOrderGainMarginOnly)
{
    eval("S = allmargin(tf(1, [1 6 11 6]));");
    EXPECT_NEAR(evalScalar("S.GainMargin(1)"),  60.0,           1e-3);
    EXPECT_NEAR(evalScalar("S.GMFrequency(1)"), 3.31662479036,  1e-5);   // sqrt(11)
    EXPECT_EQ(static_cast<int>(evalScalar("numel(S.PhaseMargin)")), 0);  // no gain crossover
    EXPECT_DOUBLE_EQ(evalScalar("double(S.Stable)"), 1.0);
}

// 1/(s(s+1)(s+2)): GM=6 at sqrt(2), PM=53.41° at 0.4457, DM=PM_rad/ω, stable.
TEST_F(AllmarginTest, IntegratorAllFields)
{
    eval("S = allmargin(tf(1, [1 3 2 0]));");
    EXPECT_NEAR(evalScalar("S.GainMargin(1)"),  6.0,          1e-4);
    EXPECT_NEAR(evalScalar("S.GMFrequency(1)"), 1.41421356,   1e-5);   // sqrt(2)
    EXPECT_NEAR(evalScalar("S.PhaseMargin(1)"), 53.4108919,   1e-3);
    EXPECT_NEAR(evalScalar("S.PMFrequency(1)"), 0.44574654,   1e-5);
    EXPECT_NEAR(evalScalar("S.DelayMargin(1)"), 2.09131387,   1e-4);
    EXPECT_NEAR(evalScalar("S.DMFrequency(1)"), 0.44574654,   1e-5);   // = PMFrequency
    EXPECT_DOUBLE_EQ(evalScalar("double(S.Stable)"), 1.0);
}

// All seven fields are present.
TEST_F(AllmarginTest, HasAllFields)
{
    eval("S = allmargin(tf(1, [1 3 2 0]));");
    EXPECT_DOUBLE_EQ(evalScalar("isfield(S,'GainMargin') & isfield(S,'GMFrequency') & "
                                "isfield(S,'PhaseMargin') & isfield(S,'PMFrequency') & "
                                "isfield(S,'DelayMargin') & isfield(S,'DMFrequency') & "
                                "isfield(S,'Stable')"), 1.0);
}

// Stable flag: closed-loop unstable plant flips it to 0.
TEST_F(AllmarginTest, StableFlagDetectsInstability)
{
    // High loop gain on a 3rd-order plant: K=100/((s+1)(s+2)(s+3)) is
    // closed-loop unstable (exceeds the gain margin of 60).
    eval("S = allmargin(tf(100, [1 6 11 6]));");
    EXPECT_DOUBLE_EQ(evalScalar("double(S.Stable)"), 0.0);
}

// zpk input routes through zp2tf. (A finite zero is used on purpose: the
// empty-zeros zpk gain-loss is a separate zp2tf bug — see
// bugs/control/zpk-empty-zeros.md. With a zero, the path is exercised
// correctly.) (s+5)/((s+1)(s+2)(s+3)) is closed-loop stable.
TEST_F(AllmarginTest, ZpkInputWithZero)
{
    eval("S = allmargin(zpk(-5, [-1 -2 -3], 1));");
    EXPECT_DOUBLE_EQ(evalScalar("double(S.Stable)"), 1.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(fieldnames(S))")), 7);
}
