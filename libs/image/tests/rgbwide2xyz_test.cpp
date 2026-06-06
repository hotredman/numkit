// libs/image/tests/rgbwide2xyz_test.cpp
//
// Regression guard for rgbwide2xyz + xyz2rgbwide — narrow-range
// BT.2020/BT.2100 RGB ↔ CIE 1931 XYZ.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WideXYZTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── BT.2020 10-bit ───────────────────────────────────────────────

TEST_F(WideXYZTest, Black10IsZero)
{
    eval("xyz = rgbwide2xyz(uint16([64 64 64]), 10);");
    EXPECT_NEAR(evalScalar("xyz(1)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("xyz(2)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("xyz(3)"), 0.0, 1e-12);
}

TEST_F(WideXYZTest, White10IsD65)
{
    eval("xyz = rgbwide2xyz(uint16([940 940 940]), 10);");
    EXPECT_NEAR(evalScalar("xyz(1)"), 0.95047, 1e-5);
    EXPECT_NEAR(evalScalar("xyz(2)"), 1.0,     1e-5);
    EXPECT_NEAR(evalScalar("xyz(3)"), 1.08883, 1e-5);
}

TEST_F(WideXYZTest, Red10)
{
    eval("xyz = rgbwide2xyz(uint16([940 64 64]), 10);");
    EXPECT_NEAR(evalScalar("xyz(1)"), 0.637010, 1e-5);
    EXPECT_NEAR(evalScalar("xyz(2)"), 0.262722, 1e-5);
    EXPECT_NEAR(evalScalar("xyz(3)"), 0.0,      1e-10);
}

TEST_F(WideXYZTest, Gray10Mid)
{
    eval("xyz = rgbwide2xyz(uint16([502 502 502]), 10);");
    EXPECT_NEAR(evalScalar("xyz(1)"), 0.246732, 1e-5);
    EXPECT_NEAR(evalScalar("xyz(2)"), 0.259589, 1e-5);
    EXPECT_NEAR(evalScalar("xyz(3)"), 0.282649, 1e-5);
}

// ── BT.2020 12-bit ──────────────────────────────────────────────

TEST_F(WideXYZTest, White12IsD65)
{
    eval("xyz = rgbwide2xyz(uint16([3760 3760 3760]), 12);");
    EXPECT_NEAR(evalScalar("xyz(1)"), 0.95047, 1e-5);
}

// ── BT.2100 ──────────────────────────────────────────────────────

TEST_F(WideXYZTest, BT2100_PQ_WhiteIsD65)
{
    eval("xyz = rgbwide2xyz(uint16([940 940 940]), 10, 'ColorSpace', 'BT.2100');");
    EXPECT_NEAR(evalScalar("xyz(1)"), 0.95047, 1e-5);
    EXPECT_NEAR(evalScalar("xyz(2)"), 1.0,     1e-5);
}

TEST_F(WideXYZTest, BT2100_HLG_WhiteIsD65)
{
    eval("xyz = rgbwide2xyz(uint16([940 940 940]), 10, "
         "'ColorSpace', 'BT.2100', 'LinearizationFcn', 'HLG');");
    EXPECT_NEAR(evalScalar("xyz(1)"), 0.95047, 1e-3);
    EXPECT_NEAR(evalScalar("xyz(2)"), 1.0,     1e-3);
}

// ── Inverse ──────────────────────────────────────────────────────

TEST_F(WideXYZTest, XYZWhiteToRGBWide)
{
    eval("rgb = xyz2rgbwide([0.95047 1 1.08883], 10);");
    EXPECT_EQ(eval("class(rgb)").toString(), "uint16");
    EXPECT_EQ(static_cast<int>(evalScalar("double(rgb(1))")), 940);
    EXPECT_EQ(static_cast<int>(evalScalar("double(rgb(2))")), 940);
    EXPECT_EQ(static_cast<int>(evalScalar("double(rgb(3))")), 940);
}

TEST_F(WideXYZTest, XYZBlackToRGBWide)
{
    eval("rgb = xyz2rgbwide([0 0 0], 10);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(rgb(1))")), 64);
    EXPECT_EQ(static_cast<int>(evalScalar("double(rgb(2))")), 64);
    EXPECT_EQ(static_cast<int>(evalScalar("double(rgb(3))")), 64);
}

TEST_F(WideXYZTest, RoundTrip10Bit)
{
    eval("orig = uint16([502 600 700]);"
         "xyz = rgbwide2xyz(orig, 10);"
         "back = xyz2rgbwide(xyz, 10);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(back(1))")), 502);
    EXPECT_EQ(static_cast<int>(evalScalar("double(back(2))")), 600);
    EXPECT_EQ(static_cast<int>(evalScalar("double(back(3))")), 700);
}

// ── Errors ──────────────────────────────────────────────────────

TEST_F(WideXYZTest, BadBPSThrows)
{
    EXPECT_THROW(eval("rgbwide2xyz(uint16([100 100 100]), 8);"),
                 std::exception);
}

TEST_F(WideXYZTest, BadColorSpaceThrows)
{
    EXPECT_THROW(
        eval("rgbwide2xyz(uint16([100 100 100]), 10, 'ColorSpace', 'BT.709');"),
        std::exception);
}
