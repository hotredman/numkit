// libs/builtin/tests/luqr_test.cpp
//
// Regression guard for builtin::lu / qr (Phase 0b).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class LuQrTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── lu ─────────────────────────────────────────────────────

TEST_F(LuQrTest, Lu3x3DecomposeIdentity)
{
    eval("A = [1 2 3; 4 5 6; 7 8 10]; [L, U, P] = lu(A);");
    // P*A == L*U
    EXPECT_NEAR(evalScalar("max(max(abs(P*A - L*U)))"), 0.0, 1e-12);
    // L is unit-lower-triangular.
    EXPECT_DOUBLE_EQ(evalScalar("L(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(3,3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(1,2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(1,3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(2,3)"), 0.0);
    // U is upper.
    EXPECT_DOUBLE_EQ(evalScalar("U(2,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("U(3,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("U(3,2)"), 0.0);
}

TEST_F(LuQrTest, LuPermutationIsValid)
{
    eval("A = [1 2 3; 4 5 6; 7 8 10]; [L, U, P] = lu(A);");
    // P must be a permutation matrix: rows and columns each sum to 1,
    // entries are 0/1.
    EXPECT_DOUBLE_EQ(evalScalar("sum(P(:))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(P(:))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("min(P(:))"), 0.0);
    // P*P' == I (orthogonal).
    EXPECT_DOUBLE_EQ(evalScalar("max(max(abs(P*P' - eye(3))))"), 0.0);
}

TEST_F(LuQrTest, LuSingleOutputCombined)
{
    // Single-output form: returns L+U combined matrix.
    eval("LU = lu([4 7; 2 6]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(LU,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(LU,2)")), 2);
}

TEST_F(LuQrTest, LuSingularRejected)
{
    EXPECT_THROW(eval("[L, U, P] = lu([1 2; 2 4]);"), std::exception);
}

TEST_F(LuQrTest, LuNonSquareRejected)
{
    EXPECT_THROW(eval("[L, U, P] = lu([1 2 3; 4 5 6]);"), std::exception);
}

// ── qr ─────────────────────────────────────────────────────

TEST_F(LuQrTest, Qr3x3FactorizationIdentity)
{
    eval("A = [1 2 3; 4 5 6; 7 8 10]; [Q, R] = qr(A);");
    // Q*R == A
    EXPECT_NEAR(evalScalar("max(max(abs(Q*R - A)))"), 0.0, 1e-12);
    // Q is orthogonal: Q'*Q == I
    EXPECT_NEAR(evalScalar("max(max(abs(Q'*Q - eye(3))))"), 0.0, 1e-12);
    // R is upper-triangular: zeros below diagonal.
    EXPECT_DOUBLE_EQ(evalScalar("R(2,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(3,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(3,2)"), 0.0);
}

TEST_F(LuQrTest, QrTallMatrix)
{
    eval("A = [1 2; 3 4; 5 6]; [Q, R] = qr(A);");
    // Q is m×m = 3×3, R is m×n = 3×2.
    EXPECT_EQ(static_cast<int>(evalScalar("size(Q,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(Q,2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,2)")), 2);
    EXPECT_NEAR(evalScalar("max(max(abs(Q*R - A)))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("max(max(abs(Q'*Q - eye(3))))"), 0.0, 1e-12);
}

TEST_F(LuQrTest, QrSingleOutputROnly)
{
    eval("R = qr([1 2 3; 4 5 6; 7 8 10]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,2)")), 3);
    // Strict lower triangle is zero (it's R, not the packed form).
    EXPECT_DOUBLE_EQ(evalScalar("R(2,1)"), 0.0);
}

TEST_F(LuQrTest, QrWideRejected)
{
    // wide matrices (m < n) require column-pivoted QR -- deferred.
    EXPECT_THROW(eval("qr([1 2 3; 4 5 6]);"), std::exception);
}
