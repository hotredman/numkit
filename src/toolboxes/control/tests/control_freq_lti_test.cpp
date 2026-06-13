// toolboxes/control/tests/control_freq_lti_test.cpp
//
// Coverage for the control frequency-response + LTI-constructor families,
// which had no gtest (parity/smoke only): freq/freq.cpp (evalfr, freqresp,
// bode, nyquist) and lti/lti.cpp (tf, zpk, ss, filt, frd, ss2ss). Reference
// values: G(s)=1/(s+1) has H(j1)=0.5-0.5j (|H|=1/sqrt2, phase -45 deg) and
// H(0)=1 (matching control_freq_smoke.m + the bode/nyquist parity specs).

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class ControlFreqLtiTest : public DualEngineTest
{};

// ── freq: evalfr / freqresp / bode / nyquist ────────────────

TEST_P(ControlFreqLtiTest, EvalfrAtJ1)
{
    eval("H = evalfr(tf(1, [1 1]), 1);");  // 1/(1+j) = 0.5 - 0.5j
    EXPECT_NEAR(evalScalar("real(H)"), 0.5, 1e-9);
    EXPECT_NEAR(evalScalar("imag(H)"), -0.5, 1e-9);
}

TEST_P(ControlFreqLtiTest, FreqrespDcGain)
{
    eval("H = freqresp(tf(1, [1 1]), 0);");  // H(0) = 1
    EXPECT_NEAR(evalScalar("real(H)"), 1.0, 1e-9);
}

TEST_P(ControlFreqLtiTest, FreqrespVectorSize)
{
    eval("H = freqresp(tf(1, [1 1]), [0 1 2]);");
    EXPECT_EQ(eval("H").numel(), 3u);
}

TEST_P(ControlFreqLtiTest, BodeAtOmega1)
{
    eval("[m, p] = bode(tf(1, [1 1]), 1);");
    EXPECT_EQ(eval("m").numel(), 1u);
    EXPECT_NEAR(evalScalar("m(1)"), 1.0 / std::sqrt(2.0), 1e-6);  // |H(j1)|
    EXPECT_NEAR(evalScalar("p(1)"), -45.0, 1e-4);                 // degrees
}

TEST_P(ControlFreqLtiTest, NyquistAtOmega1)
{
    eval("[re, im] = nyquist(tf(1, [1 1]), 1);");
    EXPECT_NEAR(evalScalar("re(1)"), 0.5, 1e-6);
    EXPECT_NEAR(evalScalar("im(1)"), -0.5, 1e-6);
}

// ── lti constructors: tf / zpk / ss / filt / frd / ss2ss ────

TEST_P(ControlFreqLtiTest, TfPoles)
{
    eval("p = sort(pole(tf([1 2], [1 3 2])));");  // s^2+3s+2 → -1,-2
    EXPECT_NEAR(evalScalar("real(p(1))"), -2.0, 1e-9);
    EXPECT_NEAR(evalScalar("real(p(2))"), -1.0, 1e-9);
}

TEST_P(ControlFreqLtiTest, ZpkPoles)
{
    eval("p = sort(real(pole(zpk([], [-1 -2], 1))));");
    EXPECT_NEAR(evalScalar("p(1)"), -2.0, 1e-9);
    EXPECT_NEAR(evalScalar("p(2)"), -1.0, 1e-9);
}

TEST_P(ControlFreqLtiTest, SsPoles)
{
    eval("p = sort(real(pole(ss([-2 0; 0 -3], [1; 1], [1 1], 0))));");
    EXPECT_NEAR(evalScalar("p(1)"), -3.0, 1e-9);
    EXPECT_NEAR(evalScalar("p(2)"), -2.0, 1e-9);
}

TEST_P(ControlFreqLtiTest, FiltDiscretePole)
{
    // filt(1,[1 -0.5]): H(z) = 1 / (1 - 0.5 z^-1) → pole at z = 0.5
    eval("p = pole(filt(1, [1 -0.5]));");
    EXPECT_NEAR(evalScalar("real(p(1))"), 0.5, 1e-9);
}

TEST_P(ControlFreqLtiTest, FrdStoresResponseAndFreq)
{
    // frd builds a freq-response-data struct {kind, Ts, resp, freq}.
    eval("f = frd([2 3], [1 2]);");
    EXPECT_NEAR(evalScalar("real(f.resp(1))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(f.resp(2))"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("f.freq(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("f.freq(2)"), 2.0, 1e-12);
}

TEST_P(ControlFreqLtiTest, Ss2ssPreservesPoles)
{
    eval("sys = ss([-2 1; 0 -3], [1; 0], [1 1], 0); T = [1 0; 1 1]; "
         "p = sort(real(pole(ss2ss(sys, T))));");
    EXPECT_NEAR(evalScalar("p(1)"), -3.0, 1e-6);
    EXPECT_NEAR(evalScalar("p(2)"), -2.0, 1e-6);
}

INSTANTIATE_DUAL(ControlFreqLtiTest);
