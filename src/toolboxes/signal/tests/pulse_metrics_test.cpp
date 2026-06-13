// toolboxes/signal/tests/pulse_metrics_test.cpp
//
// Coverage for the parity-only bilevel-waveform / transition metrics:
//   statelevels dutycycle pulsewidth pulseperiod pulsesep midcross
//   overshoot undershoot slewrate settlingtime
// Driven by a clean two-pulse rectangular waveform (low 0 / high 2, Fs=1 kHz)
// for which duty/width/period/sep are exact, plus a sharp logistic step for the
// transition metrics. Values verified against the engine.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class PulseMetricsTest : public DualEngineTest
{
protected:
    void SetUp() override
    {
        DualEngineTest::SetUp();
        // Two 200-sample high pulses at [201..400] and [601..800], Fs = 1000.
        eval("Fs = 1000; x = zeros(1000,1); x(201:400) = 2; x(601:800) = 2;");
    }
};

TEST_P(PulseMetricsTest, StateLevelsAndDuty)
{
    eval("lv = statelevels(x);");
    EXPECT_NEAR(evalScalar("lv(1)"), 0.0, 0.05);   // low state ~ 0
    EXPECT_NEAR(evalScalar("lv(2)"), 2.0, 0.05);   // high state ~ 2
    EXPECT_NEAR(evalScalar("dutycycle(x, Fs)"), 0.5, 1e-6);   // 200 high / 400 period
}

TEST_P(PulseMetricsTest, WidthPeriodSep)
{
    eval("w = pulsewidth(x, Fs); p = pulseperiod(x, Fs); s = pulsesep(x, Fs);");
    EXPECT_EQ(eval("w").numel(), 2u);
    EXPECT_NEAR(evalScalar("w(1)"), 0.2, 1e-6);    // 200 samples
    EXPECT_NEAR(evalScalar("p(1)"), 0.4, 1e-6);    // pulse-to-pulse
    EXPECT_NEAR(evalScalar("s(1)"), 0.2, 1e-6);    // gap between pulses
}

TEST_P(PulseMetricsTest, Midcross)
{
    eval("m = midcross(x, Fs);");
    EXPECT_EQ(eval("m").numel(), 4u);              // 2 rising + 2 falling
    EXPECT_NEAR(evalScalar("m(1)"), 0.2, 0.005);   // first 50% crossing ~ 0.2 s
}

TEST_P(PulseMetricsTest, StepTransitionMetrics)
{
    // Sharp monotonic logistic step 0 -> 1 centred at t = 0.5 s.
    eval("t = (0:999)'/Fs; st = 1 ./ (1 + exp(-(t - 0.5) * 200));");
    eval("o = overshoot(st, Fs);");
    EXPECT_GE(eval("o").numel(), 1u);
    EXPECT_LT(evalScalar("max(o)"), 2.0);                 // monotonic -> ~no overshoot
    EXPECT_TRUE(eval("isempty(undershoot(st, Fs))").toBool());  // monotonic -> none
    EXPECT_GT(evalScalar("slewrate(st, Fs)"), 0.0);       // positive-going edge
    eval("se = settlingtime(st, Fs, 0.05);");
    EXPECT_GT(evalScalar("se(1)"), 0.0);
    EXPECT_LT(evalScalar("se(1)"), 0.1);
}

INSTANTIATE_DUAL(PulseMetricsTest);
