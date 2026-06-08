// toolboxes/image/tests/poly2mask_test.cpp
//
// Regression guard for poly2mask — polygon scan-conversion to binary
// mask. Reference values from MATLAB R2025b, bit-exact via the
// reverse-engineered half-open ray-cast rule.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Poly2maskTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Integer-aligned square ─────────────────────────────────────────

TEST_F(Poly2maskTest, IntegerSquare)
{
    eval("M1 = poly2mask([1 3 3 1], [1 1 3 3], 5, 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(M1(:))")), 4);
    // Pixels (2,2), (2,3), (3,2), (3,3) included.
    EXPECT_EQ(static_cast<int>(evalScalar("double(M1(2,2))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M1(3,3))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M1(1,1))")), 0);
}

// ── Larger square ──────────────────────────────────────────────────

TEST_F(Poly2maskTest, LargerSquare)
{
    eval("M2 = poly2mask([2 5 5 2], [2 2 5 5], 7, 7);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(M2(:))")), 9);
}

// ── Triangle with diagonal edge ────────────────────────────────────

TEST_F(Poly2maskTest, TriangleDiagonal)
{
    eval("M3 = poly2mask([1 4 1], [1 1 4], 5, 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(M3(:))")), 3);
    // pixels (2,2), (2,3), (3,2) included.
    EXPECT_EQ(static_cast<int>(evalScalar("double(M3(2,2))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M3(2,3))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M3(3,2))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M3(3,3))")), 0);
}

// ── Fractional vertices ────────────────────────────────────────────

TEST_F(Poly2maskTest, FractionalVertices)
{
    eval("M4 = poly2mask([0.5 3.5 3.5 0.5], [0.5 0.5 3.5 3.5], 5, 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(M4(:))")), 9);
}

// ── Degenerate (zero-area) triangle ────────────────────────────────

TEST_F(Poly2maskTest, DegenerateTriangle)
{
    eval("M5 = poly2mask([1 2 1], [1 1 2], 5, 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(M5(:))")), 0);
}

// ── Self-closing polygon (last vertex == first) ────────────────────

TEST_F(Poly2maskTest, SelfClosingPolygon)
{
    eval("M6 = poly2mask([1 4 4 1 1], [1 1 4 4 1], 6, 6);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(M6(:))")), 9);
}

// ── Pentagon (irrational vertices) ─────────────────────────────────

TEST_F(Poly2maskTest, Pentagon)
{
    eval("xp = 5+[2*cos((0:4)*2*pi/5)];"
         "yp = 5+[2*sin((0:4)*2*pi/5)];"
         "M7 = poly2mask(xp, yp, 11, 11);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(M7(:))")), 10);
}

// ── Self-intersecting bowtie ───────────────────────────────────────

TEST_F(Poly2maskTest, BowtiePolygon)
{
    eval("M8 = poly2mask([1 5 3 5 1], [1 1 3 5 5], 5, 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(M8(:))")), 12);
}

// ── Large rectangle ────────────────────────────────────────────────

TEST_F(Poly2maskTest, LargeRectangle)
{
    eval("M9 = poly2mask([10 100 100 10], [10 10 80 80], 100, 110);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(M9(:))")), 6300);
}

// ── Empty inputs → all-false ───────────────────────────────────────

TEST_F(Poly2maskTest, EmptyInputs)
{
    eval("M0 = poly2mask([], [], 3, 3);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(M0(:))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(M0, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(M0, 2)")), 3);
}

// ── regionfill integration via (I, X, Y) polygon form ──────────────

TEST_F(Poly2maskTest, RegionfillPolygonIntegration)
{
    eval("I = double(reshape(1:25, 5, 5));"
         "J = regionfill(I, [2 4 4 2], [2 2 4 4]);");
    EXPECT_NEAR(evalScalar("J(3,3)"), 13.0, 1e-9);
}

// ── Errors ─────────────────────────────────────────────────────────

TEST_F(Poly2maskTest, LengthMismatchThrows)
{
    EXPECT_THROW(eval("poly2mask([1 2 3], [1 2], 5, 5);"), std::exception);
}

TEST_F(Poly2maskTest, NegativeMNThrows)
{
    EXPECT_THROW(eval("poly2mask([1 2 3], [1 2 3], -1, 5);"),
                 std::exception);
}
