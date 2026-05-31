// libs/image/tests/otf2psf_rgbwide_test.cpp
//
// Regression guards for the cycle-28 fixes / additions:
//   * otf2psf — outsize-cropping was previously ignored; now matches
//     MATLAB R2025b (shift by floor(OUTSIZE/2), then top-left crop).
//   * rgbwide2ycbcr — new function, bit-equal MATLAB R2025b on the
//     BT.2020-2 / BT.2100-2 10-bit and 12-bit narrow-range encoder.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class Otf2psfRgbWideTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── otf2psf ─────────────────────────────────────────────────────────

TEST_F(Otf2psfRgbWideTest, Otf2psfRoundtripOdd)
{
    eval("psf = [1 2 3; 4 5 6; 7 8 9];"
         "back = real(otf2psf(psf2otf(psf)));");
    for (int i = 1; i <= 3; ++i)
        for (int j = 1; j <= 3; ++j) {
            const double expected = (i - 1) * 3 + j;
            EXPECT_NEAR(evalScalar("back(" + std::to_string(i) + ","
                                   + std::to_string(j) + ")"),
                        expected, 1e-12);
        }
}

TEST_F(Otf2psfRgbWideTest, Otf2psfRoundtripEven)
{
    eval("psf = reshape(1:16, 4, 4);"
         "back = real(otf2psf(psf2otf(psf)));");
    for (int j = 1; j <= 4; ++j)
        for (int i = 1; i <= 4; ++i) {
            const double expected = (j - 1) * 4 + i;
            EXPECT_NEAR(evalScalar("back(" + std::to_string(i) + ","
                                   + std::to_string(j) + ")"),
                        expected, 1e-12);
        }
}

TEST_F(Otf2psfRgbWideTest, Otf2psfCropToOutsize)
{
    eval("otf = psf2otf([1 2; 3 4], [5 5]);"
         "back = real(otf2psf(otf, [2 2]));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(back,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(back,2)")), 2);
    EXPECT_NEAR(evalScalar("back(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("back(1,2)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("back(2,1)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("back(2,2)"), 4.0, 1e-12);
}

TEST_F(Otf2psfRgbWideTest, Otf2psf1D)
{
    eval("otf = psf2otf([1 2 3], [1 7]);"
         "back = real(otf2psf(otf, [1 3]));");
    EXPECT_EQ(static_cast<int>(evalScalar("length(back)")), 3);
    EXPECT_NEAR(evalScalar("back(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("back(2)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("back(3)"), 3.0, 1e-12);
}

TEST_F(Otf2psfRgbWideTest, Otf2psfOutsizeTooBigThrows)
{
    EXPECT_THROW(eval("otf2psf(zeros(3,3), [4 4]);"), std::exception);
}

// ── rgbwide2ycbcr ───────────────────────────────────────────────────

TEST_F(Otf2psfRgbWideTest, RgbWideYcbcr10WhiteBlack)
{
    eval("RGB = uint16([940 940 940; 64 64 64]);"
         "v = rgbwide2ycbcr(RGB, 10);");
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1)")), 940);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,2)")), 512);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,3)")), 512);
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,1)")), 64);
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,2)")), 512);
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,3)")), 512);
}

TEST_F(Otf2psfRgbWideTest, RgbWideYcbcr10Mid)
{
    eval("RGB = uint16([500 500 500]);"
         "v = rgbwide2ycbcr(RGB, 10);");
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1)")), 500);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,2)")), 512);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,3)")), 512);
}

TEST_F(Otf2psfRgbWideTest, RgbWideYcbcr10NonGray)
{
    eval("RGB = uint16([800 200 300]);"
         "v = rgbwide2ycbcr(RGB, 10);");
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1)")), 364);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,2)")), 477);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,3)")), 815);
}

TEST_F(Otf2psfRgbWideTest, RgbWideYcbcr12WhiteBlack)
{
    eval("RGB = uint16([3760 3760 3760; 256 256 256]);"
         "v = rgbwide2ycbcr(RGB, 12);");
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1)")), 3760);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,2)")), 2048);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,3)")), 2048);
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,1)")), 256);
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,2)")), 2048);
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,3)")), 2048);
}

TEST_F(Otf2psfRgbWideTest, RgbWideYcbcr12NonGray)
{
    eval("RGB = uint16([2000 1500 1000]);"
         "v = rgbwide2ycbcr(RGB, 12);");
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1)")), 1602);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,2)")), 1721);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,3)")), 2324);
}

TEST_F(Otf2psfRgbWideTest, RgbWideYcbcrImageShape)
{
    // reshape with column-major: each pixel is (R=940, G=64, B=500)
    // because the three input values go into the three pages. So
    // every pixel is the same off-gray colour.
    eval("RGBimg = uint16(reshape([940 940 940 64 64 64 500 500 500], 1, 3, 3));"
         "v = rgbwide2ycbcr(RGBimg, 10);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,3)")), 3);
    // Per the MATLAB R2025b reference probe (cf. probe_wide_test.m).
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1,1)")), 320);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1,2)")), 610);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1,3)")), 942);
}

TEST_F(Otf2psfRgbWideTest, RgbWideYcbcrBadBpsThrows)
{
    EXPECT_THROW(eval("rgbwide2ycbcr(uint16([500 500 500]), 8);"),  std::exception);
    EXPECT_THROW(eval("rgbwide2ycbcr(uint16([500 500 500]), 16);"), std::exception);
}

TEST_F(Otf2psfRgbWideTest, RgbWideYcbcrBadClassThrows)
{
    EXPECT_THROW(eval("rgbwide2ycbcr([500.0 500.0 500.0], 10);"), std::exception);
}

TEST_F(Otf2psfRgbWideTest, RgbWideYcbcrBadShapeThrows)
{
    EXPECT_THROW(eval("rgbwide2ycbcr(uint16([1 2 3 4]), 10);"),   std::exception);
    EXPECT_THROW(eval("rgbwide2ycbcr(uint16(zeros(2,2,5)), 10);"), std::exception);
}
