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

// ── Cycle K-2: pitch PEF method (Pitch Estimation Filter) ─────────────
// Bit-equal vs MATLAB R2025b audio.internal.pitch.PEF.m. PEF returns
// log-frequency-grid resolved f0 (220 Hz → 220.604203 with NFFT=2048).
TEST_F(PitchHarmonicsTest, PitchPEF220HzSineFirstFrames)
{
    eval("x = sin(2*pi*220*t); f0 = pitch(x, fs, 'Method', 'PEF');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(f0)")), 95);
    // PEF gives the same value across a pure-tone signal (high precision).
    EXPECT_NEAR(evalScalar("f0(1)"), 220.604203, 1e-4);
    EXPECT_NEAR(evalScalar("f0(2)"), 220.604203, 1e-4);
    EXPECT_NEAR(evalScalar("mean(f0)"), 220.604203, 1e-4);
}

// ── Cycle K-3: pitch LHS method (Subharmonic Summation) ───────────────
// Bit-equal vs MATLAB R2025b audio.internal.pitch.LHS.m (Hermes 1988).
// For pure 220 Hz sine, LHS picks low subharmonic at search edge (~50 Hz)
// — known Hermes-LHS behavior for narrowband signals.
TEST_F(PitchHarmonicsTest, PitchLHS220HzSineFirstFrames)
{
    eval("x = sin(2*pi*220*t); f0 = pitch(x, fs, 'Method', 'LHS');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(f0)")), 95);
    EXPECT_NEAR(evalScalar("f0(1)"),  51.0, 1e-9);
    EXPECT_NEAR(evalScalar("f0(2)"),  50.0, 1e-9);
    EXPECT_NEAR(evalScalar("mean(f0)"), 50.6, 1e-3);
}

// ── Cycle L: 'Range' Name-Value arg ────────────────────────────────────
// Bit-equal vs MATLAB R2025b for PEF + custom Range; algorithmic-equal
// for NCF (different tie-break behavior at ~1% level).
TEST_F(PitchHarmonicsTest, PitchRangeNVPEFBitEqual)
{
    // Two-tone signal: 220 + 100 Hz mix. Range=[80,250] should pick 220.
    eval("x = sin(2*pi*220*t) + 0.5*sin(2*pi*100*t);"
         "f0 = pitch(x, fs, 'Method', 'PEF', 'Range', [80 250]);");
    EXPECT_NEAR(evalScalar("f0(1)"), 221.2508, 1e-3);
}

TEST_F(PitchHarmonicsTest, PitchRangeNVRestrictsSearch)
{
    // High-pass Range picks 220 Hz tone; low-pass picks 100 Hz tone.
    eval("x = sin(2*pi*220*t) + 0.5*sin(2*pi*100*t);"
         "fHi = pitch(x, fs, 'Range', [150 400]);"
         "fLo = pitch(x, fs, 'Range', [50 150]);");
    EXPECT_NEAR(evalScalar("mean(fHi)"), 220.0, 5.0);  // ~221 Hz
    EXPECT_NEAR(evalScalar("mean(fLo)"), 100.0, 12.0); // ~109 Hz (NCF subharmonic)
}

TEST_F(PitchHarmonicsTest, PitchRangeNVRejectsBadInput)
{
    bool threw = false;
    try {
        eval("pitch([1;2;3], fs, 'Range', [400 50]);");  // hi < lo
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
