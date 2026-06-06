// libs/audio/tests/melspec_delta_test.cpp
//
// Regression guard for Audio Cycle C: melSpectrogram + audioDelta.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MelspecDeltaTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── melSpectrogram ────────────────────────────────────────────────────
TEST_F(MelspecDeltaTest, MelSpecDefaultShapeAndF)
{
    eval("[S, F, T] = melSpectrogram((1:0.1:80)', 8000);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(S, 1)")), 32);
    EXPECT_EQ(static_cast<int>(evalScalar("size(S, 2)")), 7);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(F)")), 32);
    // First and last band centers should match MATLAB's default mel grid.
    EXPECT_NEAR(evalScalar("F(1)"),  41.5811, 1e-3);
    EXPECT_NEAR(evalScalar("F(32)"), 3736.47, 1e-2);
}

TEST_F(MelspecDeltaTest, MelSpec8BandsExplicit)
{
    eval("[S, F, T] = melSpectrogram((1:0.1:80)', 8000, 8);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(S, 1)")), 8);
    EXPECT_NEAR(evalScalar("F(1)"),  164.94, 1e-2);
    EXPECT_NEAR(evalScalar("F(8)"),  3103.72, 1e-2);
    EXPECT_NEAR(evalScalar("S(1, 1)"), 0.0877958, 1e-6);
}

TEST_F(MelspecDeltaTest, MelSpecTimeAxis)
{
    eval("[S, F, T] = melSpectrogram((1:0.1:80)', 8000);"
         "winLen = round(0.03 * 8000);"
         "expectT1 = (winLen / 2) / 8000;");
    EXPECT_NEAR(evalScalar("T(1)"), evalScalar("expectT1"), 1e-9);
}

TEST_F(MelspecDeltaTest, MelSpecTooShortReturnsEmpty)
{
    eval("[S, F, T] = melSpectrogram(ones(10, 1), 8000);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(S, 2)")), 0);
}

// ── audioDelta ────────────────────────────────────────────────────────
TEST_F(MelspecDeltaTest, AudioDeltaRampDefault9)
{
    // MATLAB convention: filter with b=(M:-1:-M)/sum((1:M)^2) on ramp x=[1..10]
    // gives delta(9)=delta(10)=2 for M=4 (denom=30).
    eval("d = audioDelta((1:10)');");
    EXPECT_DOUBLE_EQ(evalScalar("d(9)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(10)"), 2.0);
}

TEST_F(MelspecDeltaTest, AudioDeltaWindowLength5)
{
    // M=2, denom = 1+4 = 5.
    // d(5) = (2*5 + 1*4 + 0*3 + (-1)*2 + (-2)*1)/5 = 10/5 = 2
    eval("d = audioDelta((1:10)', 5);");
    EXPECT_DOUBLE_EQ(evalScalar("d(5)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(10)"), 2.0);
}

TEST_F(MelspecDeltaTest, AudioDeltaMultiChannel)
{
    eval("d = audioDelta([(1:5)', (10:10:50)']);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(d, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(d, 2)")), 2);
    // Both columns scale linearly.
    EXPECT_NEAR(evalScalar("d(5, 2) / d(5, 1)"), 10.0, 1e-12);
}

TEST_F(MelspecDeltaTest, AudioDeltaRejectsEvenWindow)
{
    bool threw = false;
    try { eval("audioDelta((1:10)', 4);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(MelspecDeltaTest, AudioDeltaConstantInputZeroDelta)
{
    eval("d = audioDelta(5 * ones(10, 1));");
    // For constant input, delta should be 0 (after the filter warms up).
    EXPECT_NEAR(evalScalar("d(10)"), 0.0, 1e-12);
}

// ── Correctness: mel filterbank localises a pure tone ────────────────
// MATLAB-independent check. A correct mel filterbank, fed a pure tone,
// must concentrate its energy in the mel band whose centre frequency is
// nearest the tone, and a higher tone must peak in a higher band. This
// exercises the triangular mel filterbank end-to-end against a known
// answer (the tone frequency) rather than a reference engine.
TEST_F(MelspecDeltaTest, MelSpecLocalizesPureTone)
{
    eval("fs = 16000; t = (0:1/fs:0.5)';");

    // Low tone — 500 Hz.
    eval("xLo = sin(2*pi*500*t);"
         "[Slo, Flo, Tlo] = melSpectrogram(xLo, fs);"
         "eLo = sum(Slo, 2);"
         "[mLo, kLo] = max(eLo);");
    EXPECT_NEAR(evalScalar("Flo(kLo)"), 500.0, 300.0);   // peak band near 500 Hz
    EXPECT_GT(evalScalar("mLo / sum(eLo)"), 0.15);       // energy localised

    // High tone — 3000 Hz.
    eval("xHi = sin(2*pi*3000*t);"
         "[Shi, Fhi, Thi] = melSpectrogram(xHi, fs);"
         "eHi = sum(Shi, 2);"
         "[mHi, kHi] = max(eHi);");
    EXPECT_NEAR(evalScalar("Fhi(kHi)"), 3000.0, 500.0);
    EXPECT_GT(evalScalar("mHi / sum(eHi)"), 0.15);

    // Frequency ordering: the higher tone peaks in a higher mel band.
    EXPECT_GT(evalScalar("kHi"), evalScalar("kLo"));
}
