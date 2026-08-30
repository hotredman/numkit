// toolboxes/builtin/tests/makima_griddatan_test.cpp
//
// Regression guards for the interp cluster cycle 2:
//   - makima      — modified Akima cubic Hermite interpolation
//   - griddatan   — N-D scattered-data interpolation

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

class MakimaGriddatanTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── makima ──────────────────────────────────────────────────────────

// Hermite property: passes through the data exactly.
TEST_F(MakimaGriddatanTest, MakimaPassesThroughDataPoints)
{
    eval("x = 1:5; y = [1 4 9 16 25]; yd = makima(x, y, x);");
    EXPECT_NEAR(evalScalar("yd(1)"), 1.0,  1e-12);
    EXPECT_NEAR(evalScalar("yd(2)"), 4.0,  1e-12);
    EXPECT_NEAR(evalScalar("yd(3)"), 9.0,  1e-12);
    EXPECT_NEAR(evalScalar("yd(4)"), 16.0, 1e-12);
    EXPECT_NEAR(evalScalar("yd(5)"), 25.0, 1e-12);
}

// The modified-Akima weight (with |sum|/2 term) handles all-flat
// data without producing 0/0 — the original Akima formula would
// have w1 = w2 = 0 on m == 0 slopes.
TEST_F(MakimaGriddatanTest, MakimaConstantDataReturnsConstant)
{
    eval("yq = makima(1:5, [3 3 3 3 3], [1.5 2.5 3.5 4.5]);");
    EXPECT_NEAR(evalScalar("yq(1)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("yq(2)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("yq(3)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("yq(4)"), 3.0, 1e-12);
}

// Interpolation of x² at midpoint between nodes — Akima isn't exact
// on quadratic (it's local; pchip differs too), but should be close.
TEST_F(MakimaGriddatanTest, MakimaQuadraticInterpolationIsClose)
{
    eval("x = 1:5; y = x.^2; yq = makima(x, y, 2.5);");
    EXPECT_NEAR(evalScalar("yq"), 6.25, 0.05);
}

// 2-arg makima(x, y) now returns a pp struct usable with ppval, like
// spline/pchip (was a documented gap). Coefs verified vs MATLAB R2025b.
TEST_F(MakimaGriddatanTest, MakimaTwoArgFormReturnsPp)
{
    eval("pp = makima([1 2 3 4 5], [1 4 9 16 25]);");
    EXPECT_EQ(static_cast<int>(evalScalar("pp.order")),  4);
    EXPECT_EQ(static_cast<int>(evalScalar("pp.pieces")), 4);
    EXPECT_NEAR(evalScalar("ppval(pp, 2.5)"), 6.2395833333, 1e-9);
    // Agrees with the value form makima(x, y, xq).
    EXPECT_NEAR(evalScalar("ppval(pp, 3.3)"),
                evalScalar("makima([1 2 3 4 5], [1 4 9 16 25], 3.3)"), 1e-12);
}

// ── griddatan ───────────────────────────────────────────────────────

// 2-D linear at the centroid of unit square — for v = [1; 0; 2; 1]
// (corners (1,0) (0,0) (1,1) (0,1)) the centroid value lands on the
// triangulation edge midpoint.
TEST_F(MakimaGriddatanTest, GriddatanLinear2DAtCentroidIsConsistent)
{
    eval("X = [1 0; 0 0; 1 1; 0 1]; v = [1; 0; 2; 1];"
         "vi = griddatan(X, v, [0.5 0.5]);");
    // Average of the four corners is 1.0; either triangulation
    // diagonal lands at 1 too — both are barycentric on co-planar v.
    EXPECT_NEAR(evalScalar("vi"), 1.0, 1e-9);
}

TEST_F(MakimaGriddatanTest, GriddatanNearestPicksClosestPoint)
{
    eval("X = [1 0; 0 0; 1 1; 0 1]; v = [1; 0; 2; 1];"
         "vi = griddatan(X, v, [0.4 0.4], 'nearest');");
    // (0.4, 0.4) is closest to (0, 0) → v = 0.
    EXPECT_DOUBLE_EQ(evalScalar("vi"), 0.0);
}

TEST_F(MakimaGriddatanTest, GriddatanNearestWorksInThreeD)
{
    eval("X = [0 0 0; 1 0 0; 0 1 0; 0 0 1]; v = [1; 2; 3; 4];"
         "vi = griddatan(X, v, [0.1 0.1 0.1], 'nearest');");
    // (0.1, 0.1, 0.1) is closest to the origin → v = 1.
    EXPECT_DOUBLE_EQ(evalScalar("vi"), 1.0);
}

TEST_F(MakimaGriddatanTest, GriddatanLinearInThreeDIsKnownGap)
{
    EXPECT_THROW(eval(
        "X = [0 0 0; 1 0 0; 0 1 0; 0 0 1]; v = [1; 2; 3; 4];"
        "griddatan(X, v, [0.1 0.1 0.1], 'linear');"),
        std::exception);
}

TEST_F(MakimaGriddatanTest, GriddatanShapeMismatchThrows)
{
    EXPECT_THROW(eval(
        "X = [1 0; 0 0; 1 1; 0 1]; v = [1; 2; 3; 4];"
        "griddatan(X, v, [0.5 0.5 0.5]);"),    // xi has 3 cols, X has 2
        std::exception);
}
