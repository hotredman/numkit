// libs/image/tests/bwdistgeodesic_test.cpp
//
// Regression guard for bwdistgeodesic — binary geodesic distance
// transform via Dijkstra with chamfer edge weights. Bit-equal
// MATLAB R2025b (SINGLE precision, 1e-6 tolerance).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>
#include <cmath>

using namespace numkit;

class BwdistgeodesicTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override {
        engine.eval(
            "import compat.*;"
            "BW = true(5,5);"
            "BW(3, 1:3) = false;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── chessboard distance (default) ────────────────────────────────

TEST_F(BwdistgeodesicTest, Chessboard)
{
    eval("D = bwdistgeodesic(BW, [1 1], 'chessboard');");
    EXPECT_NEAR(evalScalar("double(D(1,5))"), 4.0, 1e-6);
    EXPECT_NEAR(evalScalar("double(D(3,4))"), 3.0, 1e-6);
    EXPECT_NEAR(evalScalar("double(D(4,1))"), 6.0, 1e-6);
    EXPECT_NEAR(evalScalar("double(D(5,5))"), 5.0, 1e-6);
}

// ── Output class is single ───────────────────────────────────────

TEST_F(BwdistgeodesicTest, OutputIsSingle)
{
    eval("D = bwdistgeodesic(BW, [1 1], 'chessboard');");
    EXPECT_EQ(eval("class(D)").toString(), "single");
}

// ── cityblock distance ───────────────────────────────────────────

TEST_F(BwdistgeodesicTest, Cityblock)
{
    eval("D = bwdistgeodesic(BW, [1 1], 'cityblock');");
    EXPECT_NEAR(evalScalar("double(D(5,1))"), 10.0, 1e-6);
    EXPECT_NEAR(evalScalar("double(D(3,4))"), 5.0,  1e-6);
}

// ── quasi-euclidean distance ─────────────────────────────────────

TEST_F(BwdistgeodesicTest, QuasiEuclidean)
{
    eval("D = bwdistgeodesic(BW, [1 1], 'quasi-euclidean');");
    EXPECT_NEAR(evalScalar("double(D(2,2))"), std::sqrt(2.0), 1e-6);
    EXPECT_NEAR(evalScalar("double(D(5,1))"), 7.656854, 1e-5);
}

// ── default == chessboard ────────────────────────────────────────

TEST_F(BwdistgeodesicTest, DefaultIsChessboard)
{
    eval("D1 = bwdistgeodesic(BW, [1 1]);"
         "D2 = bwdistgeodesic(BW, [1 1], 'chessboard');");
    // Compare at representative non-NaN positions.
    EXPECT_NEAR(evalScalar("double(D1(1,5))"),
                evalScalar("double(D2(1,5))"), 1e-6);
    EXPECT_NEAR(evalScalar("double(D1(5,5))"),
                evalScalar("double(D2(5,5))"), 1e-6);
    EXPECT_NEAR(evalScalar("double(D1(4,1))"),
                evalScalar("double(D2(4,1))"), 1e-6);
}

// ── NaN at false (barrier) pixels ────────────────────────────────

TEST_F(BwdistgeodesicTest, NaNAtBarrier)
{
    eval("D = bwdistgeodesic(BW, [1 1], 'chessboard');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(isnan(D(3,1)))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(isnan(D(3,2)))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(isnan(D(3,3)))")), 1);
}

// ── (C, R) form ─────────────────────────────────────────────────

TEST_F(BwdistgeodesicTest, ColRowForm)
{
    eval("D = bwdistgeodesic(BW, 1, 1, 'chessboard');");
    EXPECT_NEAR(evalScalar("double(D(1,5))"), 4.0, 1e-6);
}

// ── Mask form ────────────────────────────────────────────────────

TEST_F(BwdistgeodesicTest, MaskForm)
{
    eval("m = false(5,5); m(1,1)=true;"
         "D = bwdistgeodesic(BW, m, 'chessboard');");
    EXPECT_NEAR(evalScalar("double(D(1,5))"), 4.0, 1e-6);
}

// ── Unreachable region → Inf ─────────────────────────────────────

TEST_F(BwdistgeodesicTest, UnreachableInf)
{
    eval("BW2 = false(5,5); BW2(1:2,:)=true; BW2(5,:)=true;"
         "D = bwdistgeodesic(BW2, [1 1], 'chessboard');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(isinf(D(5,1)))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(isnan(D(3,3)))")), 1);
}

// ── Seed itself = 0 ──────────────────────────────────────────────

TEST_F(BwdistgeodesicTest, SeedIsZero)
{
    eval("D = bwdistgeodesic(BW, [1 1], 'chessboard');");
    EXPECT_NEAR(evalScalar("double(D(1,1))"), 0.0, 1e-9);
}

// ── Errors ─────────────────────────────────────────────────────────

TEST_F(BwdistgeodesicTest, BadMethodThrows)
{
    EXPECT_THROW(eval("bwdistgeodesic(BW, [1 1], 'bad');"),
                 std::exception);
}

TEST_F(BwdistgeodesicTest, SeedOnFalseThrows)
{
    // BW(3,1) is false; column-major 1-based linear index = (1-1)*5 + 3 = 3.
    EXPECT_THROW(eval("bwdistgeodesic(BW, 3);"), std::exception);
}
