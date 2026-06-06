// libs/image/tests/cmunique_test.cpp
//
// Regression guard for cmunique — eliminate duplicate colormap
// entries. Reference values verified bit-equal MATLAB R2025b on four
// signatures: (X, MAP) double X, (X, MAP) uint8 X, (RGB), (I).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CmuniqueTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// (X, MAP) — magic(4) with [gray(8); gray(8)] (second half is dup).
TEST_F(CmuniqueTest, XmDoubleDuplicatedCmap)
{
    eval("X = magic(4);"
         "map = [gray(8); gray(8)];"
         "[Y, newmap] = cmunique(X, map);");
    EXPECT_EQ(eval("Y").type(), ValueType::UINT8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(newmap,1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(newmap,2)")), 3);
    // MATLAB reference values for Y (0-based uint8).
    EXPECT_EQ(static_cast<int>(evalScalar("Y(1,1)")), 7);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(2,2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(4,4)")), 0);
    // newmap row 1 = [0 0 0], row 8 = [1 1 1].
    EXPECT_NEAR(evalScalar("newmap(1,1)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("newmap(8,1)"), 1.0, 1e-12);
}

// (RGB) — 2×2 truecolor.
TEST_F(CmuniqueTest, RgbThreeUniqueColours)
{
    eval("RGB = cat(3, [0.1 0.2; 0.1 0.3], [0.5 0.6; 0.5 0.7], "
         "[0.9 0.8; 0.9 0.6]);"
         "[Y, newmap] = cmunique(RGB);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(newmap,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(1,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(1,2)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(2,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(2,2)")), 0);
}

// (I) — 2×2 intensity, 3 distinct values.
TEST_F(CmuniqueTest, IntensityThreeDistinct)
{
    eval("I = [0.1 0.2; 0.1 0.3];"
         "[Y, newmap] = cmunique(I);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(newmap,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(1,1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(1,2)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(2,1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(2,2)")), 2);
}

// (X, MAP) with uint8 X — MATLAB promotes via X+1 before lookup.
TEST_F(CmuniqueTest, Uint8XDuplicateInMap)
{
    eval("X = uint8([1 2; 3 2]);"
         "map = [0 0 0; 0.5 0.5 0.5; 1 1 1; 0.5 0.5 0.5];"
         "[Y, newmap] = cmunique(X, map);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(newmap,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(1,1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(1,2)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(2,1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("Y(2,2)")), 1);
    EXPECT_NEAR(evalScalar("newmap(1,1)"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("newmap(2,1)"), 1.0, 1e-12);
}

// Output class — uint8 if newmap has ≤ 256 rows.
TEST_F(CmuniqueTest, OutputClassUint8WhenSmallCmap)
{
    eval("X = uint8([0 1; 0 1]); map = [0 0 0; 1 1 1];");
    EXPECT_EQ(eval("cmunique(X, map)").type(), ValueType::UINT8);
}

TEST_F(CmuniqueTest, EmptyIntensityImage)
{
    EXPECT_NO_THROW(eval("[Y, newmap] = cmunique(zeros(0));"));
}

// Validation: bad MAP shape.
TEST_F(CmuniqueTest, BadMapShapeThrows)
{
    EXPECT_THROW(eval("cmunique([1 2 3], [0 0; 1 1]);"), std::exception);
}

// Validation: 4-D / 5-D input is wrong.
TEST_F(CmuniqueTest, BadIntensityRankThrows)
{
    EXPECT_THROW(eval("cmunique(zeros(2,3,3,2));"), std::exception);
}

// No-duplicate cmap: newmap should equal MAP (rows reordered to
// match dedup sort order, but no rows dropped).
TEST_F(CmuniqueTest, NoDuplicatesPreservesCount)
{
    eval("X = uint8([0 1; 1 0]);"
         "map = [0 0 0; 1 1 1];"
         "[Y, newmap] = cmunique(X, map);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(newmap,1)")), 2);
}
