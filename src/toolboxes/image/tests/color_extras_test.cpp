// toolboxes/image/tests/color_extras_test.cpp
//
// Regression guard for cycle 3 image color extras: rgb2lightness +
// rgb2ind (fixed-palette nodither form).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ColorExtrasTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
    std::string evalString(const std::string &c) { return eval(c).toString(); }
};

// ── rgb2lightness ─────────────────────────────────────────────────────
TEST_F(ColorExtrasTest, RgbLightnessWhiteIs100)
{
    eval("L = rgb2lightness(uint8(255*ones(1,1,3)));");
    EXPECT_EQ(evalString("class(L)"), "single");
    EXPECT_NEAR(evalScalar("double(L(1,1))"), 100.0, 1e-3);
}

TEST_F(ColorExtrasTest, RgbLightnessBlackIs0)
{
    eval("L = rgb2lightness(uint8(zeros(1,1,3)));");
    EXPECT_NEAR(evalScalar("double(L(1,1))"), 0.0, 1e-3);
}

TEST_F(ColorExtrasTest, RgbLightnessShape)
{
    eval("L = rgb2lightness(uint8(reshape(0:11, 2, 2, 3)));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(L, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(L, 2)")), 2);
    EXPECT_EQ(evalString("class(L)"), "single");
}

TEST_F(ColorExtrasTest, RgbLightnessRejectsNon3D)
{
    bool threw = false;
    try { eval("rgb2lightness([1 2; 3 4]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// ── rgb2ind (inmap form, nodither) ────────────────────────────────────
TEST_F(ColorExtrasTest, RgbIndFixedPaletteZeroBased)
{
    // Palette: black/red/green/blue (4 colors → uint8 output, 0-based).
    eval("cmap = [0 0 0; 1 0 0; 0 1 0; 0 0 1];"
         "RGBd = double(reshape([1 0 0 0.4; 0 1 0 0; 0 0 1 0], 2, 2, 3));"
         "[X, cm] = rgb2ind(RGBd, cmap, 'nodither');");
    EXPECT_EQ(evalString("class(X)"), "uint8");
    // Pixel (1,1)=white → ties at red/green/blue (dist²=2) — pick lowest = red (0-based idx 1).
    EXPECT_DOUBLE_EQ(evalScalar("double(X(1,1))"), 1.0);
    // Pixel (1,2)=black → black (idx 0).
    EXPECT_DOUBLE_EQ(evalScalar("double(X(1,2))"), 0.0);
    // Pixel (2,1)=(0,0,0.4) → black (dist²=0.16) closer than blue (0.36).
    EXPECT_DOUBLE_EQ(evalScalar("double(X(2,1))"), 0.0);
    // Cmap echoed.
    EXPECT_EQ(static_cast<int>(evalScalar("size(cm, 1)")), 4);
}

TEST_F(ColorExtrasTest, RgbIndUint16ForLargePalette)
{
    // 300-color palette → uint16 output.
    eval("cmap = rand(300, 3);"
         "RGBd = rand(2, 2, 3);"
         "[X, cm] = rgb2ind(RGBd, cmap, 'nodither');");
    EXPECT_EQ(evalString("class(X)"), "uint16");
}

TEST_F(ColorExtrasTest, RgbIndQFormThrowsKnownGap)
{
    bool threw = false;
    try {
        eval("[X, cm] = rgb2ind(rand(2,2,3), 8);");
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(ColorExtrasTest, RgbIndDitherThrowsKnownGap)
{
    bool threw = false;
    try {
        eval("[X, cm] = rgb2ind(rand(2,2,3), [0 0 0; 1 1 1], 'dither');");
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(ColorExtrasTest, RgbIndRejectsNon3DInput)
{
    bool threw = false;
    try { eval("rgb2ind([1 2; 3 4], [0 0 0; 1 1 1]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// ── label2rgb ─────────────────────────────────────────────────────────
// DEEP-PROBE c180: numkit previously REQUIRED an explicit Nx3 map and
// rejected color-string zerocolor. MATLAB defaults map to jet(max(L(:))),
// accepts a colormap-NAME string, and accepts a ColorSpec/named zerocolor.
// All values pinned to MATLAB R2025b. Output is uint8.
TEST_F(ColorExtrasTest, Label2rgbDefaultJet)
{
    // L unique = {1,2}; jet(2) = [0 0 1; 0 1 1]; zero -> white.
    eval("y = double(label2rgb([1 2 0; 0 1 2]));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y,3)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1,1)"),   0.0);   // label1 = [0 0 255]
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1,3)"), 255.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,2,2)"), 255.0);   // label2 = [0 255 255]
    EXPECT_DOUBLE_EQ(evalScalar("y(1,2,3)"), 255.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,3,1)"), 255.0);   // zero = white
    EXPECT_DOUBLE_EQ(evalScalar("y(1,3,2)"), 255.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,3,3)"), 255.0);
}

TEST_F(ColorExtrasTest, Label2rgbJet4)
{
    // 4 labels -> jet(4) = [0 0 1; 0 1 1; 1 1 0; 1 0 0].
    eval("y = double(label2rgb([1 2 3 4]));");
    EXPECT_DOUBLE_EQ(evalScalar("y(1,3,1)"), 255.0);   // label3 = [255 255 0]
    EXPECT_DOUBLE_EQ(evalScalar("y(1,3,2)"), 255.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,3,3)"),   0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,4,1)"), 255.0);   // label4 = [255 0 0]
    EXPECT_DOUBLE_EQ(evalScalar("y(1,4,2)"),   0.0);
}

TEST_F(ColorExtrasTest, Label2rgbNamedColormap)
{
    // hsv(2) = [1 0 0; 0 1 1].
    eval("y = double(label2rgb([1 2 0; 0 1 2], 'hsv'));");
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1,1)"), 255.0);   // label1 = [255 0 0]
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1,2)"),   0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,2,2)"), 255.0);   // label2 = [0 255 255]
}

TEST_F(ColorExtrasTest, Label2rgbZeroColorString)
{
    eval("yk = double(label2rgb([1 2 0; 0 1 2], 'jet', 'k'));");
    EXPECT_DOUBLE_EQ(evalScalar("yk(1,3,1)"), 0.0);    // zero 'k' = black
    EXPECT_DOUBLE_EQ(evalScalar("yk(1,3,2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("yk(1,3,3)"), 0.0);
    eval("yr = double(label2rgb([1 2 0; 0 1 2], 'jet', 'r'));");
    EXPECT_DOUBLE_EQ(evalScalar("yr(1,3,1)"), 255.0);  // zero 'r' = red
    EXPECT_DOUBLE_EQ(evalScalar("yr(1,3,2)"),   0.0);
    eval("yg = double(label2rgb([1 2 0; 0 1 2], 'jet', 'green'));");
    EXPECT_DOUBLE_EQ(evalScalar("yg(1,3,2)"), 255.0);  // 'green' full name
    EXPECT_DOUBLE_EQ(evalScalar("yg(1,3,1)"),   0.0);
}

TEST_F(ColorExtrasTest, Label2rgbExplicitMatrixStillWorks)
{
    // Explicit Nx3 map: uint8(round(color*255)).
    eval("y = double(label2rgb([1 2], [0.1 0.2 0.3; 0.4 0.5 0.6]));");
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1,1)"),  26.0);   // round(0.1*255)
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1,3)"),  77.0);   // round(0.3*255)
    EXPECT_DOUBLE_EQ(evalScalar("y(1,2,1)"), 102.0);   // round(0.4*255)
    EXPECT_DOUBLE_EQ(evalScalar("y(1,2,3)"), 153.0);   // round(0.6*255)
}

TEST_F(ColorExtrasTest, Label2rgbShuffleDeferred)
{
    // 'shuffle' needs MATLAB's swb2712 stream — deferred, must throw.
    bool threw = false;
    try { eval("label2rgb([1 2 3 4], 'jet', 'k', 'shuffle');"); }
    catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
