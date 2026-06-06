// libs/image/tests/tonemap_test.cpp
//
// Regression guard for tonemap — Ward HDR→LDR tone mapping.
// Centre-pixel parity matches MATLAB R2025b within ±1 uint8 unit
// (accumulated floating-point drift through the
// log2/mat2gray/rgb2lab/adapthisteq/imadjust/lab2rgb pipeline).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TonemapTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override {
        engine.eval(
            "import compat.*;"
            "HDR = zeros(8, 8, 3);"
            "for c = 1:3, for r = 1:8, for cc = 1:8, "
            "  HDR(r, cc, c) = 0.001 + (r * cc * c) / 100;"
            "end, end, end;"
            "HDRg = zeros(8, 8);"
            "for r = 1:8, for cc = 1:8, "
            "  HDRg(r, cc) = 0.001 + (r * cc) / 100;"
            "end, end;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── default RGB tonemap output is uint8 H×W×3 ───────────────────

TEST_F(TonemapTest, DefaultRGB)
{
    eval("R = tonemap(HDR);");
    EXPECT_EQ(eval("class(R)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 2)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 3)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("double(R(4,4,1))")), 225);
}

// ── AdjustLightness ─────────────────────────────────────────────

TEST_F(TonemapTest, AdjustLightness)
{
    eval("R = tonemap(HDR, 'AdjustLightness', [0.1 0.9]);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(R(4,4,1))")), 225);
}

// ── AdjustSaturation ────────────────────────────────────────────

TEST_F(TonemapTest, AdjustSaturation)
{
    eval("R = tonemap(HDR, 'AdjustSaturation', 2);");
    // Saturation > 1 → red channel pulled away from grey toward
    // the colour; matches MATLAB's R(4,4,1) = 186.
    EXPECT_EQ(static_cast<int>(evalScalar("double(R(4,4,1))")), 186);
}

// ── Grayscale path ──────────────────────────────────────────────

TEST_F(TonemapTest, GrayscalePath)
{
    eval("R = tonemap(HDRg);");
    EXPECT_EQ(eval("class(R)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 2)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("double(R(4,4))")), 255);
}

// ── All-zero HDR → all-zero output ─────────────────────────────

TEST_F(TonemapTest, AllZeroHDR)
{
    eval("R = tonemap(zeros(8, 8, 3));");
    EXPECT_EQ(static_cast<int>(evalScalar("double(R(4,4,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("max(R(:))")), 0);
}

// ── Errors ──────────────────────────────────────────────────────

TEST_F(TonemapTest, NegativeHDRThrows)
{
    EXPECT_THROW(eval("tonemap(-ones(8, 8));"), std::exception);
}

TEST_F(TonemapTest, AdjustLightnessBackwardsThrows)
{
    EXPECT_THROW(eval("tonemap(HDR, 'AdjustLightness', [0.9 0.1]);"),
                 std::exception);
}

TEST_F(TonemapTest, TilesTooFewThrows)
{
    EXPECT_THROW(eval("tonemap(HDR, 'NumberOfTiles', [1 1]);"),
                 std::exception);
}
