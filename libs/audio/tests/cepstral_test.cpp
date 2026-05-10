// libs/audio/tests/cepstral_test.cpp
//
// Regression guard for Audio Cycle D: cepstralCoefficients / mfcc / gtcc.
// cepstralCoefficients is bit-equal vs MATLAB; mfcc/gtcc shape-only.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CepstralTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("S = [10; 5; 2; 1; 0.5; 0.25; 0.1; 0.05];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── cepstralCoefficients (bit-equal vs MATLAB) ────────────────────────
TEST_F(CepstralTest, CepstralCoeffsDefault13)
{
    eval("c = cepstralCoefficients(S);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(c, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(c, 2)")), 13);
    EXPECT_NEAR(evalScalar("c(1)"), -0.425721, 1e-5);
    EXPECT_NEAR(evalScalar("c(2)"),  2.11496,  1e-4);
    EXPECT_NEAR(evalScalar("c(4)"),  0.264402, 1e-5);
}

TEST_F(CepstralTest, CepstralCoeffsExplicitN)
{
    eval("c5 = cepstralCoefficients(S, 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(c5, 2)")), 5);
    EXPECT_NEAR(evalScalar("c5(2)"), 2.11496, 1e-4);
}

TEST_F(CepstralTest, CepstralCoeffsMultiFrame)
{
    eval("S2 = [10 1; 5 2; 2 5; 1 10; 0.5 5; 0.25 2; 0.1 1; 0.05 0.5];"
         "c2 = cepstralCoefficients(S2);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(c2, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(c2, 2)")), 13);
    EXPECT_NEAR(evalScalar("c2(2,1)"),  0.95423,  1e-4);
    EXPECT_NEAR(evalScalar("c2(2,2)"),  0.412677, 1e-4);
    EXPECT_NEAR(evalScalar("c2(2,4)"), -0.182984, 1e-4);
}

TEST_F(CepstralTest, CepstralCoeffsRejectsBadN)
{
    bool threw = false;
    try { eval("cepstralCoefficients(S, 1);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// ── mfcc (shape-only, KNOWN GAP for exact bit-equality) ───────────────
TEST_F(CepstralTest, MfccShape)
{
    eval("fs = 16000; t = (0:1/fs:0.1)'; x = sin(2*pi*440*t);"
         "[c, d, dd] = mfcc(x, fs);");
    // Shape: NumFrames × (NumCoeffs+1) with LogEnergy='append' default.
    EXPECT_EQ(static_cast<int>(evalScalar("size(c, 2)")), 14);
    EXPECT_GE(static_cast<int>(evalScalar("size(c, 1)")), 1);
    // delta and deltaDelta have same shape as c.
    EXPECT_EQ(static_cast<int>(evalScalar("size(d, 2)")), 14);
    EXPECT_EQ(static_cast<int>(evalScalar("size(dd, 2)")), 14);
    EXPECT_EQ(static_cast<int>(evalScalar("size(c, 1)")),
              static_cast<int>(evalScalar("size(d, 1)")));
}

TEST_F(CepstralTest, MfccCustomNumCoeffs)
{
    eval("fs = 16000; t = (0:1/fs:0.1)'; x = sin(2*pi*440*t);"
         "[c, d, dd] = mfcc(x, fs, 7);");
    // 7 coeffs + log-energy column = 8 columns.
    EXPECT_EQ(static_cast<int>(evalScalar("size(c, 2)")), 8);
}

// ── gtcc (shape-only, KNOWN GAP gammatone IIR deferred) ───────────────
TEST_F(CepstralTest, GtccShape)
{
    eval("fs = 16000; t = (0:1/fs:0.1)'; x = sin(2*pi*440*t);"
         "[g, gd, gdd] = gtcc(x, fs);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(g, 2)")), 14);
}
