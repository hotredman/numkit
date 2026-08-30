// tests/windows_test.cpp

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit;

class WindowsTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// --- hamming ---

TEST_F(WindowsTest, HammingLength)
{
    EXPECT_EQ(eval("hamming(8)").numel(), 8u);
}

TEST_F(WindowsTest, HammingEndpoints)
{
    eval("w = hamming(8);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.08, 1e-10);  // 0.54 - 0.46
    EXPECT_NEAR(evalScalar("w(8)"), 0.08, 1e-10);
}

TEST_F(WindowsTest, HammingPeakAtCenter)
{
    eval("w = hamming(9);");
    EXPECT_NEAR(evalScalar("w(5)"), 1.0, 1e-10); // center = 1.0
}

TEST_F(WindowsTest, HammingSymmetric)
{
    eval("w = hamming(8);");
    for (int i = 1; i <= 4; ++i) {
        std::string l = "w(" + std::to_string(i) + ")";
        std::string r = "w(" + std::to_string(9 - i) + ")";
        EXPECT_NEAR(evalScalar(l), evalScalar(r), 1e-10);
    }
}

// --- hanning / hann ---

TEST_F(WindowsTest, HanningLength)
{
    EXPECT_EQ(eval("hanning(16)").numel(), 16u);
}

TEST_F(WindowsTest, HanningEndpointsZero)
{
    eval("w = hanning(8);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.0, 1e-10);
    EXPECT_NEAR(evalScalar("w(8)"), 0.0, 1e-10);
}

TEST_F(WindowsTest, HannAliasMatchesHanning)
{
    eval("a = hanning(16); b = hann(16);");
    for (int i = 1; i <= 16; ++i) {
        std::string ai = "a(" + std::to_string(i) + ")";
        std::string bi = "b(" + std::to_string(i) + ")";
        EXPECT_DOUBLE_EQ(evalScalar(ai), evalScalar(bi));
    }
}

// --- blackman ---

TEST_F(WindowsTest, BlackmanLength)
{
    EXPECT_EQ(eval("blackman(32)").numel(), 32u);
}

TEST_F(WindowsTest, BlackmanEndpointsNearZero)
{
    eval("w = blackman(32);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.0, 1e-3);
    EXPECT_NEAR(evalScalar("w(32)"), 0.0, 1e-3);
}

TEST_F(WindowsTest, BlackmanSymmetric)
{
    eval("w = blackman(16);");
    for (int i = 1; i <= 8; ++i) {
        std::string l = "w(" + std::to_string(i) + ")";
        std::string r = "w(" + std::to_string(17 - i) + ")";
        EXPECT_NEAR(evalScalar(l), evalScalar(r), 1e-10);
    }
}

// --- kaiser ---

TEST_F(WindowsTest, KaiserLength)
{
    EXPECT_EQ(eval("kaiser(16, 5)").numel(), 16u);
}

TEST_F(WindowsTest, KaiserPeakAtCenter)
{
    eval("w = kaiser(9, 5);");
    EXPECT_NEAR(evalScalar("w(5)"), 1.0, 1e-10);
}

TEST_F(WindowsTest, KaiserBetaZeroIsRectangular)
{
    eval("w = kaiser(8, 0);");
    for (int i = 1; i <= 8; ++i) {
        std::string wi = "w(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(wi), 1.0, 1e-10);
    }
}

TEST_F(WindowsTest, KaiserDefaultBeta)
{
    // Default beta = 0.5 (undocumented but real).
    eval("w = kaiser(8);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.9403061933191572, 1e-12);
}

TEST_F(WindowsTest, KaiserBeta5)
{
    // beta=5 is Hamming-like.
    eval("w = kaiser(16, 5);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.0367108922712867, 1e-12);
    EXPECT_NEAR(evalScalar("w(8)"), 0.9901131036615318, 1e-12);
}

TEST_F(WindowsTest, KaiserBeta86)
{
    // beta=8.6 is Blackman-like.
    eval("w = kaiser(64, 8.6);");
    EXPECT_NEAR(evalScalar("w(1)"),  0.0013325139979024, 1e-12);
    EXPECT_NEAR(evalScalar("w(32)"), 0.9989821470221683, 1e-12);
}

TEST_F(WindowsTest, KaiserBeta12)
{
    eval("w = kaiser(64, 12);");
    EXPECT_NEAR(evalScalar("w(1)"),  0.0000527734413201, 1e-12);
    EXPECT_NEAR(evalScalar("w(32)"), 0.9985536713337064, 1e-12);
}

TEST_F(WindowsTest, KaiserSinglePoint)
{
    // Length-1 window: always [1] regardless of beta.
    eval("w = kaiser(1, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 1.0);
}

// --- rectwin ---

TEST_F(WindowsTest, RectwinAllOnes)
{
    eval("w = rectwin(10);");
    for (int i = 1; i <= 10; ++i) {
        std::string wi = "w(" + std::to_string(i) + ")";
        EXPECT_DOUBLE_EQ(evalScalar(wi), 1.0);
    }
}

// --- bartlett ---

TEST_F(WindowsTest, BartlettLength)
{
    EXPECT_EQ(eval("bartlett(16)").numel(), 16u);
}

TEST_F(WindowsTest, BartlettEndpointsZero)
{
    eval("w = bartlett(9);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.0, 1e-10);
    EXPECT_NEAR(evalScalar("w(9)"), 0.0, 1e-10);
}

TEST_F(WindowsTest, BartlettPeakAtCenter)
{
    eval("w = bartlett(9);");
    EXPECT_NEAR(evalScalar("w(5)"), 1.0, 1e-10);
}

// --- gausswin ---

TEST_F(WindowsTest, GausswinDefaultAlpha)
{
    // Default alpha = 2.5.
    eval("w = gausswin(8);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.0439369336234074, 1e-12);
    EXPECT_NEAR(evalScalar("w(4)"), 0.9382155957191078, 1e-12);
}

TEST_F(WindowsTest, GausswinNarrowAlpha)
{
    // Larger alpha -> tighter window (smaller endpoints).
    eval("w = gausswin(8, 4);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.0003354626279025, 1e-12);
}

TEST_F(WindowsTest, GausswinWideAlpha)
{
    // Smaller alpha -> wider window (larger endpoints).
    eval("w = gausswin(8, 1.5);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.3246524673583497, 1e-12);
    EXPECT_NEAR(evalScalar("w(4)"), 0.9773023728519782, 1e-12);
}

TEST_F(WindowsTest, GausswinSinglePoint)
{
    EXPECT_DOUBLE_EQ(evalScalar("gausswin(1, 4)"), 1.0);
}

// --- tukeywin ---

TEST_F(WindowsTest, TukeywinR0IsRectangular)
{
    // r=0 -> rectwin (all ones).
    eval("w = tukeywin(8, 0);");
    for (int i = 1; i <= 8; ++i)
        EXPECT_DOUBLE_EQ(evalScalar("w(" + std::to_string(i) + ")"), 1.0);
}

TEST_F(WindowsTest, TukeywinR1IsHann)
{
    // r=1 -> Hann window.
    eval("w = tukeywin(8, 1);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.0,                1e-12);
    EXPECT_NEAR(evalScalar("w(2)"), 0.1882550990706332, 1e-12);
    EXPECT_NEAR(evalScalar("w(4)"), 0.9504844339512096, 1e-12);
}

TEST_F(WindowsTest, TukeywinDefaultR)
{
    // Default r=0.5: tapers ~25% on each end.
    eval("w = tukeywin(8);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.0,             1e-12);
    EXPECT_NEAR(evalScalar("w(2)"), 0.6112604669781,  1e-9);
    EXPECT_NEAR(evalScalar("w(4)"), 1.0,              1e-12);
}

TEST_F(WindowsTest, TukeywinR025)
{
    // Small r -> mostly flat with tiny tapers.
    eval("w = tukeywin(8, 0.25);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.0,  1e-12);
    EXPECT_NEAR(evalScalar("w(2)"), 1.0,  1e-12);
}

TEST_F(WindowsTest, TukeywinSinglePoint)
{
    EXPECT_DOUBLE_EQ(evalScalar("tukeywin(1, 0.5)"), 1.0);
}
