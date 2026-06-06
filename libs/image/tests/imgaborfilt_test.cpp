// libs/image/tests/imgaborfilt_test.cpp
//
// Regression guard for imgaborfilt — single-filter Gabor in the
// frequency domain. Bit-equal MATLAB R2025b at 1e-8 (DOUBLE) /
// 1e-6 (SINGLE).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImgaborfiltTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval(
            "import compat.*;"
            "I = double(reshape(1:64, 8, 8)) / 64;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── default args (wavelength=4, orientation=0) ──────────────────

TEST_F(ImgaborfiltTest, DefaultOrient0)
{
    eval("[mag, ph] = imgaborfilt(I, 4, 0);");
    EXPECT_NEAR(evalScalar("mag(4,4)"), 0.3458250092, 1e-8);
    EXPECT_NEAR(evalScalar("mag(1,1)"), 0.9585313853, 1e-8);
    EXPECT_NEAR(evalScalar("ph(4,4)"),  0.4844512357, 1e-8);
}

// ── orientation = 90 deg ─────────────────────────────────────────

TEST_F(ImgaborfiltTest, Orient90)
{
    eval("[mag, ph] = imgaborfilt(I, 4, 90);");
    EXPECT_NEAR(evalScalar("mag(4,4)"), 0.09144046949, 1e-8);
}

// ── SpatialFrequencyBandwidth = 0.5 (wavelength 8, orient 45) ───

TEST_F(ImgaborfiltTest, CustomSFB)
{
    eval("[mag, ph] = imgaborfilt(I, 8, 45, "
         "'SpatialFrequencyBandwidth', 0.5);");
    EXPECT_NEAR(evalScalar("mag(4,4)"), 6.761782e-07, 1e-12);
}

// ── SpatialAspectRatio = 0.25 ───────────────────────────────────

TEST_F(ImgaborfiltTest, CustomSAR)
{
    eval("[mag, ph] = imgaborfilt(I, 4, 0, 'SpatialAspectRatio', 0.25);");
    EXPECT_NEAR(evalScalar("mag(4,4)"), 0.6934771743, 1e-8);
}

// ── SINGLE input class preserved ─────────────────────────────────

TEST_F(ImgaborfiltTest, SingleClass)
{
    eval("Is = single(I); [mag, ph] = imgaborfilt(Is, 4, 0);");
    EXPECT_NEAR(evalScalar("double(mag(4,4))"), 0.3458250, 1e-6);
    EXPECT_EQ(eval("class(mag)").toString(), "single");
}

// ── Output shape preserved ──────────────────────────────────────

TEST_F(ImgaborfiltTest, ShapePreserved)
{
    eval("[mag, ph] = imgaborfilt(I, 4, 0);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(mag, 1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(mag, 2)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(ph, 1)")),  8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(ph, 2)")),  8);
}

// ── Errors ──────────────────────────────────────────────────────

TEST_F(ImgaborfiltTest, BadWavelengthThrows)
{
    EXPECT_THROW(eval("imgaborfilt(I, 1, 0);"), std::exception);
}

TEST_F(ImgaborfiltTest, BadSFBThrows)
{
    EXPECT_THROW(eval("imgaborfilt(I, 4, 0, "
                      "'SpatialFrequencyBandwidth', -1);"),
                 std::exception);
}

TEST_F(ImgaborfiltTest, BadSARThrows)
{
    EXPECT_THROW(eval("imgaborfilt(I, 4, 0, "
                      "'SpatialAspectRatio', 0);"),
                 std::exception);
}
