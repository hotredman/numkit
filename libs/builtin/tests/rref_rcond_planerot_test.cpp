// libs/builtin/tests/rref_rcond_planerot_test.cpp
//
// Regression guard for linalg cycle 2:
//   rref(A [, tol])    reduced row echelon form
//   rcond(A)           reciprocal 1-norm condition estimate
//   planerot([x; y])   Givens rotation

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class RrefRcondPlanerotTest : public ::testing::Test
{
public:
    StdEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── rref ──────────────────────────────────────────────────────────────
TEST_F(RrefRcondPlanerotTest, RrefFullRankIdentity)
{
    eval("R = rref([1 2 3; 4 5 6; 7 8 10]);");
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(3,3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(3,1)"), 0.0);
}

TEST_F(RrefRcondPlanerotTest, RrefRankOneJb)
{
    eval("[R, jb] = rref([1 2 3; 2 4 6; 3 6 9]);");
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(3,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(jb)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("jb(1)"), 1.0);  // 1-based pivot
}

TEST_F(RrefRcondPlanerotTest, RrefRectangularJb)
{
    // 3×4, rank 2, pivots in columns 1 and 3.
    eval("[R, jb] = rref([1 2 0 1; 0 0 1 2; 1 2 1 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(jb)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("jb(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("jb(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(3,3)"), 0.0);
}

TEST_F(RrefRcondPlanerotTest, RrefWide)
{
    // 2×4, rank 2: leading 2×2 becomes identity, free cols filled.
    eval("R = rref([1 2 3 4; 5 6 7 8]);");
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,3)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,4)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,4)"), 3.0);
}

TEST_F(RrefRcondPlanerotTest, RrefAllZeros)
{
    eval("[R, jb] = rref([0 0 0; 0 0 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(jb)"), 0.0);  // no pivots
}

// ── rcond ─────────────────────────────────────────────────────────────
TEST_F(RrefRcondPlanerotTest, RcondIdentity)
{
    EXPECT_DOUBLE_EQ(evalScalar("rcond(eye(3))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("rcond(eye(5))"), 1.0);
}

TEST_F(RrefRcondPlanerotTest, RcondDiag)
{
    // norm(A,1) = 3, norm(inv(A),1) = 0.5, cond=1.5, rcond=2/3
    EXPECT_NEAR(evalScalar("rcond([2 0; 0 3])"), 2.0/3.0, 1e-12);
}

TEST_F(RrefRcondPlanerotTest, Rcond2x2)
{
    // [1 2; 3 4]: det=-2, inv=[-2 1; 1.5 -0.5], rcond=1/21
    EXPECT_NEAR(evalScalar("rcond([1 2; 3 4])"), 1.0/21.0, 1e-12);
}

TEST_F(RrefRcondPlanerotTest, RcondHilbert)
{
    // hilb(4) is famously ill-conditioned but well within FP precision.
    EXPECT_NEAR(evalScalar("rcond(hilb(4))"), 3.524229e-05, 1e-9);
}

TEST_F(RrefRcondPlanerotTest, RcondSingularReturnsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("rcond([1 2; 2 4])"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("rcond(zeros(3,3))"), 0.0);
}

// ── planerot ──────────────────────────────────────────────────────────
TEST_F(RrefRcondPlanerotTest, PlanerotClassic34)
{
    // [3; 4]: r=5, c=0.6, s=0.8, G=[0.6 0.8; -0.8 0.6].
    eval("[G, y] = planerot([3; 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("G(1,1)"),  0.6);
    EXPECT_DOUBLE_EQ(evalScalar("G(1,2)"),  0.8);
    EXPECT_DOUBLE_EQ(evalScalar("G(2,1)"), -0.8);
    EXPECT_DOUBLE_EQ(evalScalar("G(2,2)"),  0.6);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0.0);
}

TEST_F(RrefRcondPlanerotTest, PlanerotAlreadyAligned)
{
    // [1; 0]: G = identity, y = [1; 0].
    eval("[G, y] = planerot([1; 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("G(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("G(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("G(1,2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
}

TEST_F(RrefRcondPlanerotTest, PlanerotNegativeX)
{
    // [-3; 4]: r=5, c=-0.6, s=0.8, G=[-0.6 0.8; -0.8 -0.6].
    eval("[G, y] = planerot([-3; 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("G(1,1)"), -0.6);
    EXPECT_DOUBLE_EQ(evalScalar("G(2,2)"), -0.6);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 5.0);  // r = +hypot
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0.0);
}

TEST_F(RrefRcondPlanerotTest, PlanerotZeroDegenerate)
{
    eval("[G, y] = planerot([0; 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("G(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("G(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0.0);
}

TEST_F(RrefRcondPlanerotTest, PlanerotApplicationProducesZero)
{
    // G * [x; y] should equal [r; 0].
    eval("v = [3.7; -1.2]; [G, y] = planerot(v); res = G*v - y;");
    EXPECT_LT(std::abs(evalScalar("res(1)")), 1e-14);
    EXPECT_LT(std::abs(evalScalar("res(2)")), 1e-14);
}

// ── error / shape edges ───────────────────────────────────────────────
TEST_F(RrefRcondPlanerotTest, RcondNonSquareThrows)
{
    bool threw = false;
    try { eval("rcond([1 2 3; 4 5 6]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(RrefRcondPlanerotTest, PlanerotWrongShapeThrows)
{
    bool threw = false;
    try { eval("planerot([1 2 3]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
