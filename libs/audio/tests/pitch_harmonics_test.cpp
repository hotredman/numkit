// libs/audio/tests/pitch_harmonics_test.cpp
//
// Regression guard for Audio Cycle E (FINAL): pitch + harmonicRatio.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PitchHarmonicsTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("fs = 16000; t = (0:1/fs:1)';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── pitch ─────────────────────────────────────────────────────────────
TEST_F(PitchHarmonicsTest, Pitch220HzSineEstimate)
{
    eval("x = sin(2*pi*220*t); f0 = pitch(x, fs);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(f0)")), 95);
    // Within 1% of 220 Hz.
    EXPECT_NEAR(evalScalar("mean(f0)"), 220.0, 2.5);
}

TEST_F(PitchHarmonicsTest, Pitch100HzSineEstimate)
{
    eval("y = sin(2*pi*100*t); f0 = pitch(y, fs);");
    // Within 2% of 100 Hz.
    EXPECT_NEAR(evalScalar("mean(f0)"), 100.0, 2.0);
}

TEST_F(PitchHarmonicsTest, PitchFrameCountFormula)
{
    // numFrames = floor((N - winLen) / hop) + 1
    // For 16001 sample input with winLen=832, hop=160: 95 frames.
    eval("x = sin(2*pi*440*t); f0 = pitch(x, fs);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(f0)")), 95);
}

TEST_F(PitchHarmonicsTest, PitchTooShortReturnsEmpty)
{
    eval("x = ones(100, 1); f0 = pitch(x, fs);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(f0)")), 0);
}

// ── harmonicRatio ─────────────────────────────────────────────────────
TEST_F(PitchHarmonicsTest, HarmonicRatioSineHigh)
{
    eval("x = sin(2*pi*220*t); hr = harmonicRatio(x, fs);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(hr)")), 98);
    // Pure sine should yield high HR (close to 1).
    EXPECT_GT(evalScalar("mean(hr)"), 0.7);
}

TEST_F(PitchHarmonicsTest, HarmonicRatioInRange)
{
    eval("x = sin(2*pi*220*t); hr = harmonicRatio(x, fs);"
         "minHR = min(hr); maxHR = max(hr);");
    EXPECT_GE(evalScalar("minHR"), 0.0);
    EXPECT_LE(evalScalar("maxHR"), 1.0);
}

TEST_F(PitchHarmonicsTest, HarmonicRatioFrameCount)
{
    // For harmonicRatio: winLen=480 (30ms), overlap=320 (20ms), hop=160.
    // numFrames(16001, 480, 160) = floor((16001-480)/160)+1 = 98.
    eval("x = sin(2*pi*440*t); hr = harmonicRatio(x, fs);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(hr)")), 98);
}

TEST_F(PitchHarmonicsTest, BothEmptyOnTinyInput)
{
    eval("x = [1; 2; 3]; f = pitch(x, fs); h = harmonicRatio(x, fs);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(f)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(h)")), 0);
}

// ── Cycle K: pitch CEP method (Cepstrum) ──────────────────────────────
// Bit-equal vs MATLAB R2025b audio.internal.pitch.CEP.m for first frames
// of a 220 Hz pure sine. CEP picks integer-bin lags so the f0 values
// land on fs/k for various k near the true period.
TEST_F(PitchHarmonicsTest, PitchCEP220HzSineFirstFrames)
{
    eval("x = sin(2*pi*220*t); f0 = pitch(x, fs, 'Method', 'CEP');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(f0)")), 95);
    // First 5 bit-equal with MATLAB
    EXPECT_NEAR(evalScalar("f0(1)"), 213.333333, 1e-4);
    EXPECT_NEAR(evalScalar("f0(2)"), 246.153846, 1e-4);
    EXPECT_NEAR(evalScalar("f0(3)"), 246.153846, 1e-4);
    EXPECT_NEAR(evalScalar("f0(4)"), 216.216216, 1e-4);
    EXPECT_NEAR(evalScalar("f0(5)"), 246.153846, 1e-4);
    // Mean ~ 233.6 (cepstrum has integer-bin granularity)
    EXPECT_NEAR(evalScalar("mean(f0)"), 233.6022, 1e-3);
}

TEST_F(PitchHarmonicsTest, PitchMethodCaseInsensitive)
{
    // 'cep' / 'CEP' / 'Cep' all dispatch to CEP method.
    eval("x = sin(2*pi*220*t);"
         "a = pitch(x, fs, 'Method', 'CEP');"
         "b = pitch(x, fs, 'method', 'cep');");
    EXPECT_NEAR(evalScalar("a(1)"), evalScalar("b(1)"), 1e-12);
}
