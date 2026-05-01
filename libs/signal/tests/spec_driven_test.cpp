// libs/signal/tests/spec_driven_test.cpp
//
// Tests for D2 — spec-driven filters: lowpass / highpass / bandpass / bandstop.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit;

class SpecDrivenTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        // Build a test signal: sum of two tones (low 50 Hz + high 800 Hz),
        // sample rate 4000 Hz, 1024 samples. The exact constants are used
        // in every test so they live in SetUp.
        engine.eval("fs = 4000;");
        engine.eval("t = (0:1023)/fs;");
        engine.eval("x = sin(2*pi*50*t) + sin(2*pi*800*t);");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── lowpass: 50 Hz tone passes, 800 Hz tone is suppressed ────────────
TEST_F(SpecDrivenTest, LowpassRetainsLowTone)
{
    eval("y = lowpass(x, 200, fs);");
    // Low-tone amplitude (RMS) should be close to 1/sqrt(2) ≈ 0.707.
    EXPECT_NEAR(evalScalar("rms(y)"), 1.0 / std::sqrt(2.0), 0.05);
    // High-tone power should drop dramatically vs original RMS (~1.0).
    EXPECT_LT(evalScalar("rms(y)"), 0.85);
}

TEST_F(SpecDrivenTest, LowpassValidatesArgs)
{
    bool threw = false;
    try { eval("lowpass(x, 0, fs);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// ── highpass: 800 Hz tone passes, 50 Hz tone is suppressed ───────────
TEST_F(SpecDrivenTest, HighpassRetainsHighTone)
{
    eval("xHi = sin(2*pi*800*t);");
    eval("y = highpass(xHi, 400, fs);");
    EXPECT_NEAR(evalScalar("rms(y)"), 1.0 / std::sqrt(2.0), 0.05);
}

TEST_F(SpecDrivenTest, HighpassRejectsLowTone)
{
    eval("xLo = sin(2*pi*50*t);");
    eval("y = highpass(xLo, 400, fs);");
    EXPECT_LT(evalScalar("rms(y)"), 0.1);
}

TEST_F(SpecDrivenTest, BandpassMidband)
{
    eval("xMid = sin(2*pi*300*t);");
    eval("y = bandpass(xMid, [200 600], fs);");
    EXPECT_NEAR(evalScalar("rms(y)"), 1.0 / std::sqrt(2.0), 0.1);
}

TEST_F(SpecDrivenTest, BandpassRejectsOutOfBand)
{
    eval("xLo = sin(2*pi*50*t);");
    eval("y = bandpass(xLo, [200 600], fs);");
    EXPECT_LT(evalScalar("rms(y)"), 0.2);
}

TEST_F(SpecDrivenTest, BandpassValidatesRange)
{
    bool threw = false;
    try { eval("bandpass(x, [600 200], fs);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// ── bandstop: removes mid-band ───────────────────────────────────────
TEST_F(SpecDrivenTest, BandstopRemovesMid)
{
    eval("xMid = sin(2*pi*300*t);");
    eval("y = bandstop(xMid, [200 600], fs);");
    EXPECT_LT(evalScalar("rms(y)"), 0.3);
}

TEST_F(SpecDrivenTest, BandstopKeepsOutOfBand)
{
    eval("xLo = sin(2*pi*50*t);");
    eval("y = bandstop(xLo, [200 600], fs);");
    EXPECT_GT(evalScalar("rms(y)"), 0.5);
}
