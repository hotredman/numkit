// libs/image/tests/demosaic_test.cpp
//
// Regression guard for demosaic — MHC 2004 Bayer demosaicing.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DemosaicTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── constant input → constant output (DC preservation) ───────────

TEST_F(DemosaicTest, ConstantRGGB)
{
    eval("I = uint8(128*ones(8,8)); RGB = demosaic(I, 'rggb');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(RGB,1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(RGB,2)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(RGB,3)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,1))")), 128);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,2))")), 128);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,3))")), 128);
}

TEST_F(DemosaicTest, ConstantBGGR)
{
    eval("RGB = demosaic(uint8(128*ones(8,8)), 'bggr');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,2))")), 128);
}

TEST_F(DemosaicTest, ConstantGRBG)
{
    eval("RGB = demosaic(uint8(128*ones(8,8)), 'grbg');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,2))")), 128);
}

TEST_F(DemosaicTest, ConstantGBRG)
{
    eval("RGB = demosaic(uint8(128*ones(8,8)), 'gbrg');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,2))")), 128);
}

// ── distinguishable RGGB test ────────────────────────────────────

TEST_F(DemosaicTest, RGGB_DistinguishablePattern)
{
    // R=100 at (odd,odd), G1=50 at (odd,even), G2=60 at (even,odd),
    // B=200 at (even,even). Then values come out at MHC-correct
    // reconstructions matched against MATLAB.
    eval("I = uint8(zeros(8,8));"
         "I(1:2:end,1:2:end) = 100;"
         "I(1:2:end,2:2:end) = 50;"
         "I(2:2:end,1:2:end) = 60;"
         "I(2:2:end,2:2:end) = 200;"
         "RGB = demosaic(I, 'rggb');");
    // Interior B-pixel (4,4): R-at-B = 100, G-at-B = 55, B-orig = 200.
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,1))")), 100);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,2))")), 55);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,3))")), 200);
    // Interior R-pixel (3,3): R-orig = 100, G-at-R = 55, B-at-R = 200.
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(3,3,1))")), 100);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(3,3,2))")), 55);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(3,3,3))")), 200);
    // Boundary G-pixel (1,2): R-at-G in R-row → 95 (MATLAB-verified).
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(1,2,1))")), 95);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(2,1,1))")), 105);
}

// ── other alignments (matching MATLAB) ───────────────────────────

TEST_F(DemosaicTest, BGGR_Center44)
{
    eval("I = uint8(zeros(8,8));"
         "I(1:2:end,1:2:end) = 100;"
         "I(1:2:end,2:2:end) = 50;"
         "I(2:2:end,1:2:end) = 60;"
         "I(2:2:end,2:2:end) = 200;"
         "RGB = demosaic(I, 'bggr');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,1))")), 200);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,2))")),  55);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,3))")), 100);
}

TEST_F(DemosaicTest, GRBG_Center44)
{
    eval("I = uint8(zeros(8,8));"
         "I(1:2:end,1:2:end) = 100;"
         "I(1:2:end,2:2:end) = 50;"
         "I(2:2:end,1:2:end) = 60;"
         "I(2:2:end,2:2:end) = 200;"
         "RGB = demosaic(I, 'grbg');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,1))")), 100);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,2))")), 200);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,3))")), 110);
}

TEST_F(DemosaicTest, GBRG_Center44)
{
    eval("I = uint8(zeros(8,8));"
         "I(1:2:end,1:2:end) = 100;"
         "I(1:2:end,2:2:end) = 50;"
         "I(2:2:end,1:2:end) = 60;"
         "I(2:2:end,2:2:end) = 200;"
         "RGB = demosaic(I, 'gbrg');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,1))")), 110);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,2))")), 200);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,3))")), 100);
}

// ── gradient: smooth → identical channels ────────────────────────

TEST_F(DemosaicTest, SmoothGradient)
{
    eval("[X,Y] = meshgrid(1:8, 1:8);"
         "I = uint8(X*8 + Y);"
         "RGB = demosaic(I, 'rggb');");
    // For smooth signal MHC recovers each channel to the linear interp value.
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(3,3,1))")), 27);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(4,4,2))")), 36);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(5,5,3))")), 45);
}

// ── uint16 class preservation ───────────────────────────────────

TEST_F(DemosaicTest, Uint16ConstantClass)
{
    eval("RGB = demosaic(uint16(1000*ones(6,6)), 'rggb');");
    EXPECT_EQ(eval("class(RGB)").toString(), "uint16");
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(3,3,1))")), 1000);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(3,3,2))")), 1000);
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(3,3,3))")), 1000);
}

// ── BitsPerSample NV is accepted (no-op for clamping, per MATLAB) ─

TEST_F(DemosaicTest, BitsPerSampleAccepted)
{
    eval("RGB = demosaic(uint16(1000*ones(6,6)), 'rggb', 'BitsPerSample', 12);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(RGB(3,3,1))")), 1000);
}

// ── errors ───────────────────────────────────────────────────────

TEST_F(DemosaicTest, OddDimThrows)
{
    EXPECT_THROW(eval("demosaic(uint8(zeros(5,4)), 'rggb');"), std::exception);
}

TEST_F(DemosaicTest, BadAlignmentThrows)
{
    EXPECT_THROW(eval("demosaic(uint8(zeros(8,8)), 'badname');"), std::exception);
}

TEST_F(DemosaicTest, DoubleInputThrows)
{
    EXPECT_THROW(eval("demosaic(double(zeros(8,8)), 'rggb');"), std::exception);
}
