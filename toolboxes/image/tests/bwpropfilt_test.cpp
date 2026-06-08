// toolboxes/image/tests/bwpropfilt_test.cpp
//
// Regression guard for bwpropfilt — filter CC by region attribute.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BWPropFiltTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval(
            "import compat.*;"
            "BW = false(10, 10);"
            "BW(2:4, 2:4) = true;"     // comp 1: 9 pixels
            "BW(6:8, 2:7) = true;"     // comp 2: 18 pixels (largest)
            "BW(2:3, 7:9) = true;"     // comp 3: 6 pixels (smallest)
            "I = zeros(10, 10);"
            "I(2:4, 2:4) = 100;"
            "I(6:8, 2:7) = 200;"
            "I(2:3, 7:9) = 50;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Area filtering ──────────────────────────────────────────────

TEST_F(BWPropFiltTest, AreaRangeKeepsMiddle)
{
    eval("B = bwpropfilt(BW, 'Area', [7 15]);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(B(:))")), 9);
}

TEST_F(BWPropFiltTest, AreaTopLargest)
{
    eval("B = bwpropfilt(BW, 'Area', 1);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(B(:))")), 18);
}

TEST_F(BWPropFiltTest, AreaTopSmallestTwo)
{
    eval("B = bwpropfilt(BW, 'Area', 2, 'smallest');");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(B(:))")), 15);
}

// ── Eccentricity ────────────────────────────────────────────────

TEST_F(BWPropFiltTest, EccentricityRange)
{
    eval("B = bwpropfilt(BW, 'Eccentricity', [0.7 1.0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(B(:))")), 24);
}

// ── EulerNumber ─────────────────────────────────────────────────

TEST_F(BWPropFiltTest, EulerNumberKeepsAll)
{
    eval("B = bwpropfilt(BW, 'EulerNumber', [0 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(B(:))")), 33);
}

// ── Extent ──────────────────────────────────────────────────────

TEST_F(BWPropFiltTest, ExtentRectAreOne)
{
    eval("B = bwpropfilt(BW, 'Extent', [0.95 1.0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(B(:))")), 33);
}

// ── EquivDiameter ───────────────────────────────────────────────

TEST_F(BWPropFiltTest, EquivDiameterFiltersByArea)
{
    // comp1 (Area 9) → eqdiam ≈ 3.385; comp2 (18) → 4.787; comp3 (6) → 2.764.
    eval("B = bwpropfilt(BW, 'EquivDiameter', [3 4]);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(B(:))")), 9);  // only comp1
}

// ── ConvexArea / Solidity (== Area for convex rects) ───────────

TEST_F(BWPropFiltTest, ConvexAreaRange)
{
    eval("B = bwpropfilt(BW, 'ConvexArea', [7 20]);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(B(:))")), 27);  // 9 + 18
}

TEST_F(BWPropFiltTest, SolidityNearOne)
{
    eval("B = bwpropfilt(BW, 'Solidity', [0.95 1.0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(B(:))")), 33);
}

// ── Marker-aware: MeanIntensity ────────────────────────────────

TEST_F(BWPropFiltTest, MeanIntensityFiltersByBrightness)
{
    eval("B = bwpropfilt(BW, I, 'MeanIntensity', [150 255]);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(B(:))")), 18);
}

// ── CC struct input ────────────────────────────────────────────

TEST_F(BWPropFiltTest, CCStructInputFiltersOneComponent)
{
    eval("CC = bwconncomp(BW, 8);"
         "CC2 = bwpropfilt(CC, 'Area', 1);");
    EXPECT_EQ(static_cast<int>(evalScalar("CC2.NumObjects")), 1);
}

// ── Empty image ────────────────────────────────────────────────

TEST_F(BWPropFiltTest, EmptyImageNoComponents)
{
    eval("B = bwpropfilt(false(5,5), 'Area', 1);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(B(:))")), 0);
}

// ── Errors ─────────────────────────────────────────────────────

TEST_F(BWPropFiltTest, BadAttributeThrows)
{
    EXPECT_THROW(eval("bwpropfilt(BW, 'Banana', 1);"), std::exception);
}

TEST_F(BWPropFiltTest, MarkerNeededForIntensityAttribute)
{
    EXPECT_THROW(eval("bwpropfilt(BW, 'MeanIntensity', [150 255]);"),
                 std::exception);
}

TEST_F(BWPropFiltTest, BadConnThrows)
{
    EXPECT_THROW(eval("bwpropfilt(BW, 'Area', 1, 'largest', 5);"),
                 std::exception);
}

TEST_F(BWPropFiltTest, BadDirThrows)
{
    EXPECT_THROW(eval("bwpropfilt(BW, 'Area', 1, 'medium');"),
                 std::exception);
}
