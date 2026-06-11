// toolboxes/linalg/tests/qr_pivoting_test.cpp
//
// Regression guard for bugs/linalg/qr-pivoting.md (FIXED): [Q,R,P] = qr(A) is
// column-pivoted Householder QR (A*P = Q*R, columns pivoted by decreasing
// norm); [Q,R,p] = qr(A,'vector') returns a permutation vector; econ + 3 outputs
// returns P as a vector (MATLAB convention). Validation is sign-agnostic on R
// (Q signs may differ from MATLAB by a reflection — the QR is still valid).
// MATLAB R2025b reference.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class QrPivotingTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Tall 3x2 repro: P swaps the columns, A*P = Q*R, exact R (tall → no trailing
// 1x1 sign quirk).
TEST_F(QrPivotingTest, Tall3x2)
{
    eval("A = [1 2; 3 4; 5 6]; [Q, R, P] = qr(A);");
    EXPECT_DOUBLE_EQ(evalScalar("P(1,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(1,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(2,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(2,2)"), 0.0);
    EXPECT_NEAR(evalScalar("R(1,1)"), -7.483315, 1e-5);   // tall → exact match
    EXPECT_LT(evalScalar("max(max(abs(A*P - Q*R)))"), 1e-10);
    EXPECT_LT(evalScalar("max(max(abs(Q'*Q - eye(3))))"), 1e-10);
}

// 'vector' option returns the permutation vector p (1-based).
TEST_F(QrPivotingTest, VectorOption)
{
    eval("A = [1 2; 3 4; 5 6]; [Q, R, p] = qr(A, 'vector');");
    EXPECT_DOUBLE_EQ(evalScalar("p(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("p(2)"), 1.0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(p,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(p,2)")), 2);
}

// 3x3 square: column order [3 1 2], |diag(R)| decreasing, reconstruction.
TEST_F(QrPivotingTest, Square3x3)
{
    eval("B = [1 2 3; 4 5 6; 7 8 10]; [Q, R, p] = qr(B, 'vector');");
    EXPECT_DOUBLE_EQ(evalScalar("p(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("p(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("p(3)"), 2.0);
    EXPECT_NEAR(evalScalar("abs(R(1,1))"), 12.041595, 1e-4);
    EXPECT_NEAR(evalScalar("abs(R(2,2))"), 1.053735, 1e-4);
    EXPECT_NEAR(evalScalar("abs(R(3,3))"), 0.236433, 1e-4);
    // |diag(R)| is non-increasing (rank-revealing property)
    EXPECT_GE(evalScalar("abs(R(1,1))"), evalScalar("abs(R(2,2))"));
    EXPECT_GE(evalScalar("abs(R(2,2))"), evalScalar("abs(R(3,3))"));
    eval("[Q2, R2, P2] = qr(B);");  // matrix form
    EXPECT_LT(evalScalar("max(max(abs(B*P2 - Q2*R2)))"), 1e-10);
}

// econ + 3 outputs → P is a vector; Q is m×min(m,n).
TEST_F(QrPivotingTest, EconVectorP)
{
    eval("A = [1 2; 3 4; 5 6]; [Q, R, P] = qr(A, 0);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,1)")), 1);   // vector
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(Q,2)")), 2);   // economy Q
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,1)")), 2);
}

// A larger tall case: exact R + reconstruction + pivot order.
TEST_F(QrPivotingTest, Tall4x3)
{
    eval("A = [1 5 2; 4 2 8; 1 1 3; 7 0 6]; [Q, R, p] = qr(A, 'vector');");
    EXPECT_LT(evalScalar("max(max(abs(A(:,p) - Q*R)))"), 1e-10);
    EXPECT_LT(evalScalar("max(max(abs(Q'*Q - eye(4))))"), 1e-10);
    EXPECT_GE(evalScalar("abs(R(1,1))"), evalScalar("abs(R(2,2))"));
    EXPECT_GE(evalScalar("abs(R(2,2))"), evalScalar("abs(R(3,3))"));
}

// Plain 2-output qr is unchanged.
TEST_F(QrPivotingTest, TwoOutputUnchanged)
{
    eval("A = [1 2; 3 4; 5 6]; [Q, R] = qr(A);");
    EXPECT_NEAR(evalScalar("R(1,1)"), -5.916080, 1e-5);   // unpivoted
    EXPECT_LT(evalScalar("max(max(abs(Q*R - A)))"), 1e-10);
}
