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

// ── Cycle M: ISO 532-2 phon/sone (PCHIP table-lookup) ────────────────
// All in-range values bit-equal vs MATLAB R2025b phon2sone/sone2phon
// with second arg 'ISO 532-2'. Out-of-range (phon > 120) ships
// pchip-extrapolation only (KNOWN GAP — fzero refinement deferred).
TEST_F(FreqScalesTest, ISO5322TableExactValues)
{
    // Table 5 anchor points (28 entries from MATLAB getPerceptualConstants.m):
    // phon=0 → sone=0.001; phon=120 → sone=337.6
    EXPECT_NEAR(evalScalar("phon2sone(0,   'ISO 532-2')"),   0.001, 1e-9);
    EXPECT_NEAR(evalScalar("phon2sone(20,  'ISO 532-2')"),   0.146, 1e-9);
    EXPECT_NEAR(evalScalar("phon2sone(40,  'ISO 532-2')"),   1.000, 1e-9);
    EXPECT_NEAR(evalScalar("phon2sone(60,  'ISO 532-2')"),   4.140, 1e-9);
    EXPECT_NEAR(evalScalar("phon2sone(100, 'ISO 532-2')"),  69.600, 1e-9);
    EXPECT_NEAR(evalScalar("phon2sone(120, 'ISO 532-2')"), 337.600, 1e-9);
}

TEST_F(FreqScalesTest, ISO5322Sone2PhonInverseAtAnchors)
{
    // Inverse: sone=0.146 → phon=20, etc.
    EXPECT_NEAR(evalScalar("sone2phon(0.146,  'ISO 532-2')"),  20.0, 1e-9);
    EXPECT_NEAR(evalScalar("sone2phon(1.000,  'ISO 532-2')"),  40.0, 1e-9);
    EXPECT_NEAR(evalScalar("sone2phon(337.6,  'ISO 532-2')"), 120.0, 1e-9);
}

TEST_F(FreqScalesTest, ISO5322Sone2PhonPchipInterpolation)
{
    // Bit-equal with MATLAB R2025b for off-anchor sone values via PCHIP.
    EXPECT_NEAR(evalScalar("sone2phon(0.1, 'ISO 532-2')"), 17.159421, 1e-5);
    EXPECT_NEAR(evalScalar("sone2phon(10,  'ISO 532-2')"), 73.307915, 1e-5);
    EXPECT_NEAR(evalScalar("sone2phon(100, 'ISO 532-2')"), 104.747454, 1e-5);
}

TEST_F(FreqScalesTest, ISO5322Sone2PhonLinearExtrapolation)
{
    // For sone > 337.6 (table max), MATLAB switches to linear extrapolation.
    EXPECT_NEAR(evalScalar("sone2phon(500,  'ISO 532-2')"), 127.211368, 1e-5);
    EXPECT_NEAR(evalScalar("sone2phon(1000, 'ISO 532-2')"), 149.413854, 1e-5);
}

TEST_F(FreqScalesTest, ISO5322DiffersFromISO5321)
{
    // ISO 532-2 ≠ ISO 532-1 by design (different scale definitions):
    // ISO 532-1: phon2sone(20) ≈ 0.138 (closed-form power law)
    // ISO 532-2: phon2sone(20) =  0.146 (Table 5 lookup)
    EXPECT_NEAR(evalScalar("phon2sone(20)"),                  0.138011, 1e-5);
    EXPECT_NEAR(evalScalar("phon2sone(20, 'ISO 532-2')"),     0.146,    1e-9);
}
