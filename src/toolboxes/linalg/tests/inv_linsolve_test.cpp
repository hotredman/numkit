// toolboxes/builtin/tests/inv_linsolve_test.cpp
//
// Regression guard for builtin::inv / linsolve / pageinv.
// All three share the la_solve LU/QR backend.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class InvLinsolveTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── inv ─────────────────────────────────────────────────────

TEST_F(InvLinsolveTest, Inv2x2)
{
    eval("A = [4 7; 2 6]; B = inv(A);");
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"),  0.6);
    EXPECT_DOUBLE_EQ(evalScalar("B(1,2)"), -0.7);
    EXPECT_DOUBLE_EQ(evalScalar("B(2,1)"), -0.2);
    EXPECT_DOUBLE_EQ(evalScalar("B(2,2)"),  0.4);
    EXPECT_NEAR(evalScalar("max(abs(reshape(A*B - eye(2), 4, 1)))"), 0.0, 1e-14);
}

TEST_F(InvLinsolveTest, Inv3x3SatisfiesIdentity)
{
    eval("A = [1 2 3; 0 1 4; 5 6 0]; B = inv(A);");
    EXPECT_NEAR(evalScalar("max(abs(reshape(A*B - eye(3), 9, 1)))"), 0.0, 1e-12);
}

TEST_F(InvLinsolveTest, InvIdentityRoundtrip)
{
    eval("I = eye(5); B = inv(I);");
    EXPECT_DOUBLE_EQ(evalScalar("max(max(abs(B - eye(5))))"), 0.0);
}

TEST_F(InvLinsolveTest, InvSingularRejected)
{
    EXPECT_THROW(eval("inv([1 2; 2 4]);"), std::exception);  // rank 1
}

TEST_F(InvLinsolveTest, InvNonSquareRejected)
{
    EXPECT_THROW(eval("inv([1 2; 3 4; 5 6]);"), std::exception);
}

// ── linsolve ────────────────────────────────────────────────

TEST_F(InvLinsolveTest, LinsolveSquareSystem)
{
    eval("A = [1 2 3; 4 5 6; 7 8 10]; b = [6; 15; 25]; x = linsolve(A, b);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("max(abs(A*x - b))"), 0.0, 1e-12);
}

TEST_F(InvLinsolveTest, LinsolveTallLeastSquares)
{
    // Tall full-rank system -- least-squares via QR.
    eval("A = [1 2; 3 4; 5 6]; b = [3; 7; 11]; x = linsolve(A, b);");
    // Exact solution x = [1; 1] (b = A*[1;1] exactly).
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 1.0, 1e-12);
}

TEST_F(InvLinsolveTest, LinsolveMultiRhs)
{
    eval("A = [4 7; 2 6]; B = [1 0; 0 1]; X = linsolve(A, B);");
    // X = inv(A) -- verified per earlier inv test.
    EXPECT_DOUBLE_EQ(evalScalar("X(1,1)"),  0.6);
    EXPECT_DOUBLE_EQ(evalScalar("X(2,2)"),  0.4);
}

TEST_F(InvLinsolveTest, LinsolveBadDimsRejected)
{
    EXPECT_THROW(eval("linsolve([1 2; 3 4], [1; 2; 3]);"), std::exception);
}

// ── pageinv ─────────────────────────────────────────────────

TEST_F(InvLinsolveTest, Pageinv2D)
{
    // 2D input: same as inv(A).
    eval("A = [4 7; 2 6]; B = pageinv(A);");
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"),  0.6);
    EXPECT_DOUBLE_EQ(evalScalar("B(2,2)"),  0.4);
}

TEST_F(InvLinsolveTest, Pageinv3D)
{
    eval("P = zeros(2, 2, 3);"
         "P(:,:,1) = [4 7; 2 6];"
         "P(:,:,2) = eye(2);"
         "P(:,:,3) = [2 0; 0 4];"
         "Q = pageinv(P);");
    // Page 1: general 2x2.
    EXPECT_DOUBLE_EQ(evalScalar("Q(1,1,1)"),  0.6);
    EXPECT_DOUBLE_EQ(evalScalar("Q(2,2,1)"),  0.4);
    // Page 2: identity inverse is identity.
    EXPECT_DOUBLE_EQ(evalScalar("Q(1,1,2)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("Q(2,2,2)"),  1.0);
    // Page 3: diagonal inverse.
    EXPECT_DOUBLE_EQ(evalScalar("Q(1,1,3)"),  0.5);
    EXPECT_DOUBLE_EQ(evalScalar("Q(2,2,3)"),  0.25);
}

TEST_F(InvLinsolveTest, PageinvSingularRejected)
{
    eval("P = zeros(2, 2, 2); P(:,:,1) = eye(2); P(:,:,2) = [1 2; 2 4];");  // page 2 singular
    EXPECT_THROW(eval("pageinv(P);"), std::exception);
}
