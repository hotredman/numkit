// libs/image/tests/regionfill_test.cpp
//
// Regression guard for regionfill — discrete Laplacian inpainting.
// Reference values from MATLAB R2025b. CG solver tol 1e-12 yields
// the same machine-precision Laplacian fix-point as MATLAB's
// sparse-direct (UMFPACK) approach.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RegionfillTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval(
            "import compat.*;"
            "I = double(reshape(1:25, 5, 5));"
            "m1 = false(5,5); m1(3,3) = true;"
            "m2 = false(5,5); m2(2:4, 2:4) = true;"
            "I3 = double(magic(10));"
            "m3 = false(10,10); m3(4:7, 4:7) = true;"
            "m4 = false(10,10); m4(1:3, 5:7) = true;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Single-pixel interior mask ─────────────────────────────────────

TEST_F(RegionfillTest, SinglePixelInterior)
{
    eval("J1 = regionfill(I, m1);");
    // Avg of N+S+W+E = 12+14+8+18 = 52/4 = 13.
    EXPECT_NEAR(evalScalar("J1(3,3)"), 13.0, 1e-9);
}

// ── 3x3 interior mask — linear field preserved ─────────────────────

TEST_F(RegionfillTest, ThreeByThreeMaskInterior)
{
    eval("J2 = regionfill(I, m2);");
    // Column-major 1..25 is linear in c, so filling a Laplacian-
    // smooth region recovers the original values exactly.
    EXPECT_NEAR(evalScalar("J2(2,2)"), 7.0, 1e-9);
    EXPECT_NEAR(evalScalar("J2(3,3)"), 13.0, 1e-9);
    EXPECT_NEAR(evalScalar("J2(4,4)"), 19.0, 1e-9);
}

// ── 4x4 mask on magic(10) — non-linear interior ────────────────────

TEST_F(RegionfillTest, MagicSquareInterior)
{
    eval("J3 = regionfill(I3, m3);");
    EXPECT_NEAR(evalScalar("J3(5,5)"), 46.11363636, 1e-7);
    EXPECT_NEAR(evalScalar("J3(4,4)"), 29.11363636, 1e-7);
    EXPECT_NEAR(evalScalar("J3(6,6)"), 48.88636364, 1e-7);
    EXPECT_NEAR(evalScalar("J3(4,7)"), 57.75, 1e-9);
}

// ── Edge-touching mask (3-neighbour stencil on top row) ────────────

TEST_F(RegionfillTest, EdgeTouchingMask)
{
    eval("J4 = regionfill(I3, m4);");
    EXPECT_NEAR(evalScalar("J4(1,5)"), 22.45003013, 1e-7);
    EXPECT_NEAR(evalScalar("J4(2,6)"), 37.90377113, 1e-7);
}

// ── Empty-mask passthrough ─────────────────────────────────────────

TEST_F(RegionfillTest, EmptyMaskReturnsInput)
{
    eval("J0 = regionfill(I, false(5,5));");
    EXPECT_NEAR(evalScalar("J0(1,1)"), 1.0,  1e-12);
    EXPECT_NEAR(evalScalar("J0(3,3)"), 13.0, 1e-12);
    EXPECT_NEAR(evalScalar("J0(5,5)"), 25.0, 1e-12);
}

// ── Class preservation ─────────────────────────────────────────────

TEST_F(RegionfillTest, Uint8Class)
{
    eval("Iu = uint8(I); Ju = regionfill(Iu, m1);");
    // J1(3,3) == 13 → uint8 13.
    EXPECT_EQ(static_cast<int>(evalScalar("double(Ju(3,3))")), 13);
    EXPECT_EQ(eval("class(Ju)").toString(), "uint8");
}

// ── Errors ─────────────────────────────────────────────────────────

TEST_F(RegionfillTest, ImageTooSmallThrows)
{
    EXPECT_THROW(eval("regionfill(zeros(2,2), false(2,2));"),
                 std::exception);
}

TEST_F(RegionfillTest, MaskSizeMismatchThrows)
{
    EXPECT_THROW(eval("regionfill(I, false(4,4));"), std::exception);
}

TEST_F(RegionfillTest, PolygonForm)
{
    // (I, X, Y) form goes through poly2mask, then mask-form solve.
    eval("Jp = regionfill(I, [2 4 4 2], [2 2 4 4]);");
    EXPECT_NEAR(evalScalar("Jp(3,3)"), 13.0, 1e-9);
}
