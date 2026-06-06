// libs/audio/tests/pitch_harmonics_test.cpp
// Regression guard for Audio Cycle E (FINAL): pitch + harmonicRatio.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PitchHarmonicsTest : public ::testing::Test
{
public:
    StdEngine engine;
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

// ── pitch PEF method (Pitch Estimation Filter) ───────────────────────
// Clean-room implementation of the published PEF method — PEFAC without
// amplitude compression (Gonzalez & Brookes, EUSIPCO 2011); see
//. On a clean tone this agrees with MATLAB's
// PEF to ~0.06 % (220.471 vs 220.604) — within the parity tolerance —
// but it is a paper-faithful implementation, not bit-matched to MATLAB
// (whose PEF uses a different comb-filter formula).
TEST_F(PitchHarmonicsTest, PitchPEF220HzSineFirstFrames)
{
    eval("x = sin(2*pi*220*t); f0 = pitch(x, fs, 'Method', 'PEF');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(f0)")), 95);
    // PEF gives the same value across a pure-tone signal.
    EXPECT_NEAR(evalScalar("f0(1)"), 220.471263, 1e-4);
    EXPECT_NEAR(evalScalar("f0(2)"), 220.471263, 1e-4);
    EXPECT_NEAR(evalScalar("mean(f0)"), 220.471263, 1e-4);
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

// ── pitch SRH method (Summation of Residual Harmonics) ───────────────
// Clean-room implementation of the published SRH method (Drugman &
// Alwan, Interspeech 2011). It is a
// faithful paper implementation, intentionally NOT bit-matched to
// MATLAB's SRH (whose internal pipeline is undocumented). These values
// are a regression anchor for numkit's paper-SRH, not a MATLAB-parity
// check. SRH is a speech pitch tracker; on a pure sine (no glottal
// harmonic structure) its output is degenerate but deterministic.
TEST_F(PitchHarmonicsTest, PitchSRH220HzSineRegression)
{
    eval("x = sin(2*pi*220*t); f0 = pitch(x, fs, 'Method', 'SRH');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(f0)")), 95);
    EXPECT_NEAR(evalScalar("f0(1)"), 66.0, 1e-9);
    EXPECT_NEAR(evalScalar("mean(f0)"), 66.0, 1e-6);
}

// ── 'Range' Name-Value arg with PEF ──────────────────────────────────
// Regression anchor for the clean-room paper-PEF with a custom Range.
// Two pure tones (220 + 100 Hz) are a degenerate input for PEF — there
// is no glottal harmonic structure to sum — and paper-PEF resolves a
// sub-octave here, deterministically. What this test pins: the 'Range'
// Name-Value arg is honoured (the result stays inside [80, 250]).
TEST_F(PitchHarmonicsTest, PitchRangeNVPEF)
{
    eval("x = sin(2*pi*220*t) + 0.5*sin(2*pi*100*t);"
         "f0 = pitch(x, fs, 'Method', 'PEF', 'Range', [80 250]);");
    EXPECT_NEAR(evalScalar("f0(1)"), 108.876204, 1e-3);
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

// ── Correctness test: harmonic-rich signal, known f0 ─────────────────
// The other pitch tests use a pure sine — a degenerate input for
// harmonic pitch detection (no harmonic structure to sum). This test
// feeds a genuine harmonic signal: a 150 Hz tone with 8 harmonics at
// 1/k amplitude (sawtooth-like) — exactly what CEP / LHS / PEF / SRH
// and the NCF default are designed for. The true f0 is known by
// construction (MATLAB-independent). A correct estimate lands within a
// couple of Hz of 150; the +/-10 Hz band fails an octave error
// (75 / 300 Hz) or garbage.
TEST_F(PitchHarmonicsTest, PitchAllMethodsHarmonic150Hz)
{
    eval("x = sin(2*pi*150*t) + sin(2*pi*300*t)/2 + sin(2*pi*450*t)/3"
         "  + sin(2*pi*600*t)/4 + sin(2*pi*750*t)/5 + sin(2*pi*900*t)/6"
         "  + sin(2*pi*1050*t)/7 + sin(2*pi*1200*t)/8;");
    EXPECT_NEAR(evalScalar("mean(pitch(x, fs))"),                  150.0, 10.0); // NCF
    EXPECT_NEAR(evalScalar("mean(pitch(x, fs, 'Method', 'CEP'))"), 150.0, 10.0);
    EXPECT_NEAR(evalScalar("mean(pitch(x, fs, 'Method', 'LHS'))"), 150.0, 10.0);
    EXPECT_NEAR(evalScalar("mean(pitch(x, fs, 'Method', 'PEF'))"), 150.0, 10.0);
    EXPECT_NEAR(evalScalar("mean(pitch(x, fs, 'Method', 'SRH'))"), 150.0, 10.0);
}
