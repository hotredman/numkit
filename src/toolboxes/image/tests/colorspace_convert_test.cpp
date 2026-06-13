// toolboxes/image/tests/colorspace_convert_test.cpp
//
// Coverage for parity-only colour-space conversions + utilities:
//   rgb2xyz xyz2rgb xyz2lab lab2xyz          (CIE conversions)
//   rgb2lin lin2rgb rgb2ntsc ntsc2rgb        (gamma / luma)
//   lab2double lab2single lab2uint8 lab2uint16
//   xyz2double xyz2uint16                    (scale casts)
//   whitepoint deltaE wavelength2rgb colorgradient roicolor
// Conversion pairs are checked by exact round-trip; absolute anchors use known
// references (D65 white, sRGB decode of 0.5 = 0.214, luma of pure red = 0.299).

#include "dual_engine_fixture.hpp"

using namespace m_test;

class ColorspaceConvertTest : public DualEngineTest
{};

TEST_P(ColorspaceConvertTest, WhitepointXyzLab)
{
    eval("wp = whitepoint('d65');");
    EXPECT_NEAR(evalScalar("wp(1)"), 0.9505, 1e-3);
    EXPECT_NEAR(evalScalar("wp(2)"), 1.0, 1e-9);
    EXPECT_NEAR(evalScalar("wp(3)"), 1.0888, 1e-3);
    // White RGB maps to the reference white in XYZ.
    eval("x1 = rgb2xyz([1 1 1]);");
    EXPECT_NEAR(evalScalar("x1(1)"), 0.9505, 1e-3);
    EXPECT_NEAR(evalScalar("x1(3)"), 1.0888, 1e-3);
    // round-trips
    EXPECT_LT(evalScalar("max(abs(xyz2rgb(rgb2xyz([0.4 0.6 0.8])) - [0.4 0.6 0.8]))"), 1e-6);
    EXPECT_LT(evalScalar("max(abs(lab2xyz(xyz2lab([0.3 0.4 0.5])) - [0.3 0.4 0.5]))"), 1e-10);
}

TEST_P(ColorspaceConvertTest, GammaAndNtsc)
{
    EXPECT_NEAR(evalScalar("rgb2lin(0.5)"), 0.214041, 1e-5);     // sRGB decode
    EXPECT_LT(evalScalar("abs(lin2rgb(rgb2lin(0.6)) - 0.6)"), 1e-9);
    eval("n = rgb2ntsc([1 0 0]);");
    EXPECT_NEAR(evalScalar("n(1)"), 0.2989, 1e-3);               // luma of pure red
    EXPECT_LT(evalScalar("max(abs(ntsc2rgb(rgb2ntsc([0.2 0.5 0.7])) - [0.2 0.5 0.7]))"), 1e-9);
}

TEST_P(ColorspaceConvertTest, ScaleCasts)
{
    EXPECT_DOUBLE_EQ(evalScalar("isa(lab2single([50 20 -30]), 'single')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isa(lab2uint8([50 20 -30]), 'uint8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isa(lab2uint16([50 20 -30]), 'uint16')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isa(xyz2uint16([0.5 0.5 0.5]), 'uint16')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isa(xyz2double(uint16([100 200 300])), 'double')"), 1.0);
    // round-trip through uint8 recovers L (a*,b* exact, L* ~ within a quantum).
    eval("ld = lab2double(lab2uint8([50 20 -30]));");
    EXPECT_NEAR(evalScalar("ld(1)"), 50.0, 0.3);
    EXPECT_NEAR(evalScalar("ld(2)"), 20.0, 1e-6);
    EXPECT_NEAR(evalScalar("ld(3)"), -30.0, 1e-6);
}

TEST_P(ColorspaceConvertTest, MetricsAndMaps)
{
    EXPECT_DOUBLE_EQ(evalScalar("deltaE([50 20 30], [50 20 30])"), 0.0);  // identical
    EXPECT_GT(evalScalar("deltaE([50 20 30], [55 25 35])"), 0.0);
    eval("w = wavelength2rgb(550);");
    EXPECT_EQ(eval("w").numel(), 3u);
    EXPECT_DOUBLE_EQ(evalScalar("all(w >= 0 & w <= 1)"), 1.0);
    // colorgradient interpolates between stacked colors.
    eval("g = colorgradient([1 0 0; 0 0 1], 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(g,1)")), 5);
    EXPECT_DOUBLE_EQ(evalScalar("g(1,1)"), 1.0);   // starts red
    EXPECT_DOUBLE_EQ(evalScalar("g(5,3)"), 1.0);   // ends blue
    // roicolor: logical mask of in-range pixels.
    eval("r = roicolor([1 2 3; 4 5 6], 2, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("islogical(r)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(r(:))"), 4.0);   // {2,3,4,5}
}

INSTANTIATE_DUAL(ColorspaceConvertTest);
