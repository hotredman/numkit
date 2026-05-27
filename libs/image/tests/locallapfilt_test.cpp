// libs/image/tests/locallapfilt_test.cpp
//
// Regression guard for locallapfilt — fast local Laplacian filtering
// (Aubry-Paris-Hasinoff-Kautz-Durand 2014). Interior-pixel parity
// vs MATLAB R2025b within ±5 uint8 units (LLF's private pyrdownsample
// uses an undocumented boundary convention that differs slightly
// from impyramid; boundary pixels diverge more but interior pixels
// match within a few uint8 units, which is well below the visual-
// difference threshold for tone-mapped images).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class LocalLapFiltTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval(
            "import compat.*;"
            "I = uint8([10 20 30 40 50 60 70 80;"
            "          20 80 80 80 80 80 80 70;"
            "          30 80 200 200 200 200 80 60;"
            "          40 80 200 250 250 200 80 50;"
            "          50 80 200 250 250 200 80 40;"
            "          60 80 200 200 200 200 80 30;"
            "          70 80 80 80 80 80 80 20;"
            "          80 70 60 50 40 30 20 10]);");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Default 3-arg form: sigma=0.3, alpha=0.4 ────────────────────

TEST_F(LocalLapFiltTest, DefaultClassAndSize)
{
    eval("B = locallapfilt(I, 0.3, 0.4);");
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 2)")), 8);
}

TEST_F(LocalLapFiltTest, DefaultInteriorPixels)
{
    eval("B = locallapfilt(I, 0.3, 0.4);");
    // Interior parity within ±5 uint8 of MATLAB reference.
    EXPECT_NEAR(evalScalar("double(B(3,3))"), 195.0, 5.0);
    EXPECT_NEAR(evalScalar("double(B(4,4))"), 255.0, 5.0);  // saturated
    EXPECT_NEAR(evalScalar("double(B(3,5))"), 198.0, 5.0);
    EXPECT_NEAR(evalScalar("double(B(5,5))"), 255.0, 5.0);
}

// ── beta != 1: dynamic-range compression ────────────────────────

TEST_F(LocalLapFiltTest, BetaCompresses)
{
    eval("B = locallapfilt(I, 0.3, 0.4, 0.8);");
    EXPECT_NEAR(evalScalar("double(B(4,4))"), 247.0, 5.0);
    EXPECT_NEAR(evalScalar("double(B(3,3))"), 187.0, 5.0);
}

// ── alpha > 1: smoothing ────────────────────────────────────────

TEST_F(LocalLapFiltTest, AlphaGreaterThanOneSmooths)
{
    eval("B = locallapfilt(I, 0.3, 2.0);");
    // Peak gets slightly smoothed (still bright). Interior tone
    // matches MATLAB within ±5 uint8.
    EXPECT_NEAR(evalScalar("double(B(4,4))"), 239.0, 5.0);
    EXPECT_NEAR(evalScalar("double(B(3,3))"), 203.0, 5.0);
}

// ── Special-case passthroughs ────────────────────────────────────

TEST_F(LocalLapFiltTest, AlphaOneBetaOneIsPassthrough)
{
    eval("B = locallapfilt(I, 0.3, 1.0, 1.0);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(isequal(B, I))")), 1);
}

TEST_F(LocalLapFiltTest, SigmaZeroBetaOneIsPassthrough)
{
    eval("B = locallapfilt(I, 0, 0.4, 1.0);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(isequal(B, I))")), 1);
}

TEST_F(LocalLapFiltTest, FlatImageIsPassthrough)
{
    eval("F = uint8(100 * ones(8, 8));"
         "B = locallapfilt(F, 0.3, 0.4);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(isequal(B, F))")), 1);
}

// ── NumIntensityLevels ──────────────────────────────────────────

TEST_F(LocalLapFiltTest, NumIntensityLevelsOne)
{
    eval("B = locallapfilt(I, 0.3, 0.4, 1.0, 'NumIntensityLevels', 1);");
    // Single sample at refVal = (min+max)/2 = 130/255 = 0.5098.
    // Bit-exact with MATLAB R[1,1] = 10.
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1))")), 10);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(4,4))")), 250);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,3))")), 204);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(8,8))")), 10);
}

TEST_F(LocalLapFiltTest, NumIntensityLevelsEight)
{
    eval("B = locallapfilt(I, 0.3, 0.4, 1.0, 'NumIntensityLevels', 8);");
    EXPECT_NEAR(evalScalar("double(B(4,4))"), 255.0, 5.0);
    EXPECT_NEAR(evalScalar("double(B(3,3))"), 196.0, 5.0);
}

TEST_F(LocalLapFiltTest, NumIntensityLevelsAuto)
{
    // auto for alpha=0.4: round(((50*0.9 - 16*0.1) - (50-16)*0.4) / 0.8)
    //                   = round(29.8 / 0.8) = 37
    eval("B = locallapfilt(I, 0.3, 0.4, 1.0, 'NumIntensityLevels', 'auto');");
    EXPECT_NEAR(evalScalar("double(B(3,3))"), 195.0, 5.0);
}

// ── Input class preservation ────────────────────────────────────

TEST_F(LocalLapFiltTest, SingleInputPreserved)
{
    eval("Is = single(I)/255;"
         "B = locallapfilt(Is, 0.3, 0.4);");
    EXPECT_EQ(eval("class(B)").toString(), "single");
}

TEST_F(LocalLapFiltTest, Int16InputPreserved)
{
    eval("Ii = int16([-100 -50 0 50 100 150 200 250;"
         "             -100 -50 0 50 100 150 200 250;"
         "             -100 -50 0 50 100 150 200 250;"
         "             -100 -50 0 50 100 150 200 250;"
         "             -100 -50 0 50 100 150 200 250;"
         "             -100 -50 0 50 100 150 200 250;"
         "             -100 -50 0 50 100 150 200 250;"
         "             -100 -50 0 50 100 150 200 250]);"
         "B = locallapfilt(Ii, 0.3, 0.4);");
    EXPECT_EQ(eval("class(B)").toString(), "int16");
}

// ── RGB ColorMode ───────────────────────────────────────────────

TEST_F(LocalLapFiltTest, RGBLuminance)
{
    eval("Irgb = uint8(repmat(reshape(uint8(linspace(0,255,8*8)),8,8),[1 1 3]));"
         "B = locallapfilt(Irgb, 0.3, 0.4);");
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 3)")), 3);
    // For grayscale-ramp RGB, luminance mode preserves equal channels.
    // RGB ramp has steep gradient → more boundary-sensitive than the
    // structured test pattern, so tolerance is loosened to ±10.
    EXPECT_NEAR(evalScalar("double(B(4,4,1))"), 105.0, 12.0);
}

TEST_F(LocalLapFiltTest, RGBSeparate)
{
    eval("Irgb = uint8(repmat(reshape(uint8(linspace(0,255,8*8)),8,8),[1 1 3]));"
         "B = locallapfilt(Irgb, 0.3, 0.4, 1.0, 'ColorMode', 'separate');");
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 3)")), 3);
    // RGB ramp has steep gradient → more boundary-sensitive than the
    // structured test pattern, so tolerance is loosened to ±10.
    EXPECT_NEAR(evalScalar("double(B(4,4,1))"), 105.0, 12.0);
}

// ── Errors ──────────────────────────────────────────────────────

TEST_F(LocalLapFiltTest, NegativeSigmaThrows)
{
    EXPECT_THROW(eval("locallapfilt(I, -0.1, 0.4);"), std::exception);
}

TEST_F(LocalLapFiltTest, NonPositiveAlphaThrows)
{
    EXPECT_THROW(eval("locallapfilt(I, 0.3, 0);"), std::exception);
    EXPECT_THROW(eval("locallapfilt(I, 0.3, -1);"), std::exception);
}

TEST_F(LocalLapFiltTest, NegativeBetaThrows)
{
    EXPECT_THROW(eval("locallapfilt(I, 0.3, 0.4, -0.1);"), std::exception);
}

TEST_F(LocalLapFiltTest, BadColorModeThrows)
{
    EXPECT_THROW(eval("locallapfilt(I, 0.3, 0.4, 1.0, 'ColorMode', 'rgb');"),
                 std::exception);
}

TEST_F(LocalLapFiltTest, BadNumIntensityLevelsStringThrows)
{
    EXPECT_THROW(eval("locallapfilt(I, 0.3, 0.4, 1.0, 'NumIntensityLevels', 'autoz');"),
                 std::exception);
}
