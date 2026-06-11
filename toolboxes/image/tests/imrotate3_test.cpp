// toolboxes/image/tests/imrotate3_test.cpp
//
// Regression guard for imrotate3 — 3-D volumetric rotation around
// an arbitrary axis via the Rodrigues formula.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Imrotate3Test : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("A = reshape(double(1:27), 3, 3, 3);");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── 90° axis-aligned (exact, no interpolation) ───────────────────

TEST_F(Imrotate3Test, Z90Loose)
{
    eval("B = imrotate3(A, 90, [0 0 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,3)")), 3);
    EXPECT_NEAR(evalScalar("B(1,1,1)"),  7.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,2,2)"), 14.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,3,3)"), 21.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(1,3,1)"),  9.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,1,1)"),  1.0, 1e-12);
}

TEST_F(Imrotate3Test, X90Loose)
{
    eval("B = imrotate3(A, 90, [1 0 0]);");
    EXPECT_NEAR(evalScalar("B(1,1,1)"),  3.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,2,2)"), 14.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,3,3)"), 25.0, 1e-12);
}

TEST_F(Imrotate3Test, Y90Loose)
{
    eval("B = imrotate3(A, 90, [0 1 0]);");
    EXPECT_NEAR(evalScalar("B(1,1,1)"), 19.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,2,2)"), 14.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,3,3)"),  9.0, 1e-12);
}

// ── 45° z with interpolation ─────────────────────────────────────

TEST_F(Imrotate3Test, Z45LooseLinear)
{
    eval("B = imrotate3(A, 45, [0 0 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,3)")), 3);
    EXPECT_NEAR(evalScalar("B(3,3,2)"), 14.0, 1e-12);    // centre
    EXPECT_NEAR(evalScalar("B(3,3,3)"), 23.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(1,1,1)"),  0.0, 1e-12);    // out-of-bounds fill
}

TEST_F(Imrotate3Test, Z45Crop)
{
    eval("B = imrotate3(A, 45, [0 0 1], 'linear', 'crop');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,3)")), 3);
    EXPECT_NEAR(evalScalar("B(2,2,2)"), 14.0, 1e-12);
}

// ── oblique axis ─────────────────────────────────────────────────

TEST_F(Imrotate3Test, Oblique60_111)
{
    eval("B = imrotate3(A, 60, [1 1 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,3)")), 5);
    EXPECT_NEAR(evalScalar("B(3,3,3)"), 14.0, 1e-12);
}

// ── methods ──────────────────────────────────────────────────────

TEST_F(Imrotate3Test, NearestZ30)
{
    eval("B = imrotate3(A, 30, [0 0 1], 'nearest');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 5);
    EXPECT_NEAR(evalScalar("B(3,3,2)"), 14.0, 1e-12);
}

TEST_F(Imrotate3Test, CubicZ30)
{
    eval("B = imrotate3(A, 30, [0 0 1], 'cubic');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 5);
    EXPECT_NEAR(evalScalar("B(3,3,2)"), 14.0, 1e-9);
}

// ── identity (angle = 0) ─────────────────────────────────────────

TEST_F(Imrotate3Test, AngleZeroIdentity)
{
    eval("B = imrotate3(A, 0, [0 0 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 3);
    EXPECT_NEAR(evalScalar("B(1,1,1)"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,2,2)"), 14.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,3,3)"), 27.0, 1e-12);
}

// ── FillValues NV pair ───────────────────────────────────────────

TEST_F(Imrotate3Test, FillValuesNV)
{
    eval("B = imrotate3(A, 45, [0 0 1], 'linear', 'loose', 'FillValues', -99);");
    EXPECT_NEAR(evalScalar("B(1,1,1)"), -99.0, 1e-12);
}

// ── uint8 preserved ──────────────────────────────────────────────

TEST_F(Imrotate3Test, Uint8Preserved)
{
    eval("A8 = uint8(reshape(1:27, 3, 3, 3));"
         "B = imrotate3(A8, 90, [0 0 1]);");
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,1))")),  7);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,3,3))")), 21);
}

// ── errors ───────────────────────────────────────────────────────

TEST_F(Imrotate3Test, ZeroAxisThrows)
{
    EXPECT_THROW(eval("imrotate3(A, 30, [0 0 0]);"), std::exception);
}

TEST_F(Imrotate3Test, BadMethodThrows)
{
    EXPECT_THROW(eval("imrotate3(A, 30, [0 0 1], 'gibberish');"), std::exception);
}

TEST_F(Imrotate3Test, BadBboxThrows)
{
    EXPECT_THROW(eval("imrotate3(A, 30, [0 0 1], 'linear', 'gibberish');"), std::exception);
}

TEST_F(Imrotate3Test, BadAxisLengthThrows)
{
    EXPECT_THROW(eval("imrotate3(A, 30, [1 0]);"), std::exception);
}
