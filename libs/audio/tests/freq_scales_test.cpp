// libs/audio/tests/freq_scales_test.cpp
//
// Regression guard for Audio Toolbox frequency-scale and loudness
// conversions (cycle A).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class FreqScalesTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Mel ───────────────────────────────────────────────────────────────
TEST_F(FreqScalesTest, MelOshaughnessy)
{
    EXPECT_NEAR(evalScalar("hz2mel(1000)"), 999.985537, 1e-4);
    EXPECT_NEAR(evalScalar("mel2hz(1500)"), 1949.30965, 1e-3);
}

TEST_F(FreqScalesTest, MelRoundTrip)
{
    EXPECT_NEAR(evalScalar("mel2hz(hz2mel(440))"), 440.0, 1e-9);
    EXPECT_NEAR(evalScalar("mel2hz(hz2mel(0))"),     0.0, 1e-9);
}

// ── Bark ──────────────────────────────────────────────────────────────
TEST_F(FreqScalesTest, BarkTraunmuller)
{
    EXPECT_NEAR(evalScalar("hz2bark(1000)"), 8.527432, 1e-5);
    EXPECT_NEAR(evalScalar("bark2hz(8)"),   914.595186, 1e-3);
}

TEST_F(FreqScalesTest, BarkLowFreqCorrection)
{
    // For bark < 2, MATLAB applies bark = 0.85*bark + 0.3.
    // At hz=50: raw bark = 26.81*50/2010 - 0.53 = 0.137. After correction:
    // 0.85*0.137 + 0.3 = 0.4165.
    EXPECT_NEAR(evalScalar("hz2bark(50)"), 0.4165, 5e-3);
}

TEST_F(FreqScalesTest, BarkRoundTrip)
{
    EXPECT_NEAR(evalScalar("bark2hz(hz2bark(440))"), 440.0, 1e-6);
    EXPECT_NEAR(evalScalar("bark2hz(hz2bark(2000))"), 2000.0, 1e-6);
}

// ── ERB ───────────────────────────────────────────────────────────────
TEST_F(FreqScalesTest, ErbGlasbergMoore)
{
    EXPECT_NEAR(evalScalar("hz2erb(1000)"), 15.59273873, 1e-5);
    EXPECT_NEAR(evalScalar("erb2hz(15)"),   923.9498363, 1e-3);
}

TEST_F(FreqScalesTest, ErbRoundTrip)
{
    EXPECT_NEAR(evalScalar("erb2hz(hz2erb(440))"), 440.0, 1e-9);
    EXPECT_NEAR(evalScalar("hz2erb(0)"), 0.0, 1e-12);
}

// ── Phon / sone ───────────────────────────────────────────────────────
TEST_F(FreqScalesTest, PhonSoneAnchorPoints)
{
    EXPECT_DOUBLE_EQ(evalScalar("phon2sone(40)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("phon2sone(50)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("phon2sone(60)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("sone2phon(1)"),  40.0);
    EXPECT_DOUBLE_EQ(evalScalar("sone2phon(2)"),  50.0);
    EXPECT_DOUBLE_EQ(evalScalar("sone2phon(4)"),  60.0);
}

TEST_F(FreqScalesTest, PhonSoneSubAnchorBranch)
{
    // For phon < 40: sone = (phon/40)^(1/0.35).
    EXPECT_NEAR(evalScalar("phon2sone(20)"), 0.138011189, 1e-7);
    // For sone < 1: phon = 40 * sone^0.35.
    EXPECT_NEAR(evalScalar("sone2phon(0.25)"), 24.62288827, 1e-5);
}

TEST_F(FreqScalesTest, PhonSoneRoundTripAtAnchors)
{
    EXPECT_NEAR(evalScalar("sone2phon(phon2sone(40))"), 40.0, 1e-9);
    EXPECT_NEAR(evalScalar("sone2phon(phon2sone(60))"), 60.0, 1e-9);
}

// ── Vector input ──────────────────────────────────────────────────────
TEST_F(FreqScalesTest, VectorInputElementwise)
{
    eval("v = hz2mel([100 1000 4000]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(v)")), 3);
    EXPECT_NEAR(evalScalar("v(2)"), 999.985537, 1e-4);
}

TEST_F(FreqScalesTest, EmptyInputEmptyOutput)
{
    eval("v = hz2mel([]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(v)")), 0);
}
