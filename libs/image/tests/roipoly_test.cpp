// libs/image/tests/roipoly_test.cpp
//
// Regression guard for roipoly — programmatic polygon ROI mask.
// Reference values from MATLAB R2025b, bit-exact across all 4
// non-interactive signatures and the multi-output variants.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RoipolyTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;"
                    "A = zeros(5,5); xi = [1 4 4 1]; yi = [1 1 4 4];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── 3-arg (A, xi, yi) ──────────────────────────────────────────────

TEST_F(RoipolyTest, ImageBased)
{
    eval("BW = roipoly(A, xi, yi);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(BW(:))")), 9);
}

// ── 4-arg (M, N, xi, yi) ───────────────────────────────────────────

TEST_F(RoipolyTest, SizeBased)
{
    eval("BW = roipoly(5, 5, xi, yi);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(BW(:))")), 9);
}

// ── 5-arg world coordinates ────────────────────────────────────────

TEST_F(RoipolyTest, WorldCoords5Arg)
{
    eval("BW = roipoly([0 10], [0 10], A, [2 8 8 2], [2 2 8 8]);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(BW(:))")), 9);
}

// ── 5-arg identity extents matches 3-arg ───────────────────────────

TEST_F(RoipolyTest, WorldCoordsIdentity)
{
    eval("BW = roipoly([1 5], [1 5], A, xi, yi);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(BW(:))")), 9);
}

// ── 6-arg (x, y, M, N, xi, yi) ─────────────────────────────────────

TEST_F(RoipolyTest, WorldCoords6Arg)
{
    eval("BW = roipoly([0 10], [0 10], 5, 5, [2 8 8 2], [2 2 8 8]);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(BW(:))")), 9);
}

// ── 2-output with auto-closed xi ───────────────────────────────────

TEST_F(RoipolyTest, TwoOutputAutoClose)
{
    eval("[BW, xi2] = roipoly(A, xi, yi);");
    EXPECT_EQ(static_cast<int>(evalScalar("length(xi2)")), 5);
    // xi2(end) == xi(1) (polygon auto-closes).
    EXPECT_NEAR(evalScalar("xi2(end)"), 1.0, 1e-12);
    EXPECT_EQ(static_cast<int>(evalScalar("sum(BW(:))")), 9);
}

// ── 5-output (xdata, ydata, BW, xi, yi) ────────────────────────────

TEST_F(RoipolyTest, FiveOutput)
{
    eval("[xo, yo, BW, xi3, yi3] = roipoly(A, xi, yi);");
    EXPECT_NEAR(evalScalar("xo(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("xo(2)"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("yo(2)"), 5.0, 1e-12);
    EXPECT_EQ(static_cast<int>(evalScalar("sum(BW(:))")), 9);
}

// ── Pre-closed polygon (no auto-close double-add) ──────────────────

TEST_F(RoipolyTest, PreClosedPolygon)
{
    eval("xc = [1 4 4 1 1]; yc = [1 1 4 4 1];"
         "[BW, xi3] = roipoly(A, xc, yc);");
    EXPECT_EQ(static_cast<int>(evalScalar("length(xi3)")), 5);
}

// ── Errors ─────────────────────────────────────────────────────────

TEST_F(RoipolyTest, InteractiveFormThrows)
{
    EXPECT_THROW(eval("roipoly(A);"), std::exception);
    EXPECT_THROW(eval("roipoly(5, 5);"), std::exception);
}

TEST_F(RoipolyTest, MismatchedXiYiThrows)
{
    EXPECT_THROW(eval("roipoly(A, [1 2 3], [1 2]);"), std::exception);
}
