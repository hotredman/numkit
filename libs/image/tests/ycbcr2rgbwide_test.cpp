// libs/image/tests/ycbcr2rgbwide_test.cpp
//
// Regression guard for ycbcr2rgbwide (inverse of rgbwide2ycbcr).
// Reference values from MATLAB R2025b verified bit-equal on a
// dozen probe vectors; this fixture also asserts the round-trip
// rgbwide2ycbcr → ycbcr2rgbwide recovers every input pixel exactly
// (uint16 quantisation may introduce ±1 ulp at the very edges).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class YcbcrRgbWideTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── 10-bit basic patterns ───────────────────────────────────────────

TEST_F(YcbcrRgbWideTest, Decode10Grey)
{
    eval("v = ycbcr2rgbwide(uint16([940 512 512; 64 512 512; 500 512 512]), 10);");
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1)")), 940);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,2)")), 940);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,3)")), 940);
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,1)")),  64);
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,2)")),  64);
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,3)")),  64);
    EXPECT_EQ(static_cast<int>(evalScalar("v(3,1)")), 500);
}

TEST_F(YcbcrRgbWideTest, Decode10OffGrey)
{
    // Same probe input MATLAB uses; output should be the original (800, 200, 300)
    // up to ±1 from quantisation. MATLAB returns (801, 200, 300).
    eval("v = ycbcr2rgbwide(uint16([364 477 815]), 10);");
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1)")), 801);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,2)")), 200);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,3)")), 300);
}

// ── 12-bit basic patterns ───────────────────────────────────────────

TEST_F(YcbcrRgbWideTest, Decode12Grey)
{
    eval("v = ycbcr2rgbwide(uint16([3760 2048 2048; 256 2048 2048]), 12);");
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1)")), 3760);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,3)")), 3760);
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,1)")), 256);
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,2)")), 256);
}

TEST_F(YcbcrRgbWideTest, Decode12OffGrey)
{
    eval("v = ycbcr2rgbwide(uint16([1602 1721 2324]), 12);");
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1)")), 2000);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,2)")), 1500);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,3)")), 1001);     // ±1 quant
}

// ── H×W×3 image shape ───────────────────────────────────────────────

TEST_F(YcbcrRgbWideTest, ImageShape)
{
    eval("YCC = uint16(reshape([320 320 320 610 610 610 942 942 942], 1, 3, 3));"
         "v = ycbcr2rgbwide(YCC, 10);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,3)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1,1)")), 940);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1,2)")),  64);
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1,3)")), 500);
}

// ── Round-trip ───────────────────────────────────────────────────────

TEST_F(YcbcrRgbWideTest, RoundTrip10Bit)
{
    eval("RGB = double([940 940 940; 64 64 64; 500 500 500; 800 200 300]);"
         "RGBr = double(ycbcr2rgbwide(rgbwide2ycbcr(uint16(RGB), 10), 10));"
         "d = max(max(abs(RGBr - RGB)));");
    EXPECT_LE(static_cast<int>(evalScalar("d")), 1);
}

TEST_F(YcbcrRgbWideTest, RoundTrip12Bit)
{
    eval("RGB = double([3760 3760 3760; 256 256 256; 2000 1500 1000; 3000 800 1200]);"
         "RGBr = double(ycbcr2rgbwide(rgbwide2ycbcr(uint16(RGB), 12), 12));"
         "d = max(max(abs(RGBr - RGB)));");
    EXPECT_LE(static_cast<int>(evalScalar("d")), 1);
}

// ── Validation ───────────────────────────────────────────────────────

TEST_F(YcbcrRgbWideTest, BadBpsThrows)
{
    EXPECT_THROW(eval("ycbcr2rgbwide(uint16([500 500 500]), 8);"),  std::exception);
    EXPECT_THROW(eval("ycbcr2rgbwide(uint16([500 500 500]), 16);"), std::exception);
}

TEST_F(YcbcrRgbWideTest, BadClassThrows)
{
    EXPECT_THROW(eval("ycbcr2rgbwide([500.0 500.0 500.0], 10);"), std::exception);
}

TEST_F(YcbcrRgbWideTest, BadShapeThrows)
{
    EXPECT_THROW(eval("ycbcr2rgbwide(uint16([1 2 3 4]), 10);"),   std::exception);
    EXPECT_THROW(eval("ycbcr2rgbwide(uint16(zeros(2,2,5)), 10);"), std::exception);
}
