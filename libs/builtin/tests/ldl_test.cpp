// libs/builtin/tests/ldl_test.cpp
//
// Regression guard for ldl — block LDL' factorization (v1: Crout
// LDL' without pivoting).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class LdlTest : public ::testing::Test
{
public:
    Engine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Positive-definite path ────────────────────────────────────────────
TEST_F(LdlTest, PositiveDefinite3x3FactorEntries)
{
    eval("A = [4 2 1; 2 5 3; 1 3 6];"
         "[L, D] = ldl(A);");
    // L is unit lower triangular.
    EXPECT_DOUBLE_EQ(evalScalar("L(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(2,1)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("L(3,1)"), 0.25);
    EXPECT_DOUBLE_EQ(evalScalar("L(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(3,2)"), 0.625);
    EXPECT_DOUBLE_EQ(evalScalar("L(3,3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(1,2)"), 0.0);  // upper triangle is zero
    EXPECT_DOUBLE_EQ(evalScalar("L(2,3)"), 0.0);
    // D is diagonal.
    EXPECT_DOUBLE_EQ(evalScalar("D(1,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(2,2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(3,3)"), 4.1875);
    EXPECT_DOUBLE_EQ(evalScalar("D(1,2)"), 0.0);
}

TEST_F(LdlTest, PositiveDefiniteResidual)
{
    eval("A = [4 2 1; 2 5 3; 1 3 6];"
         "[L, D] = ldl(A);"
         "res = max(max(abs(A - L*D*L')));");
    EXPECT_LT(evalScalar("res"), 1e-12);
}

TEST_F(LdlTest, ThreeOutputPMatrixIsIdentity)
{
    // v1: P is always identity (no pivoting).
    eval("A = [4 2 1; 2 5 3; 1 3 6];"
         "[L, D, P] = ldl(A);");
    EXPECT_DOUBLE_EQ(evalScalar("P(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(3,3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(1,2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(2,1)"), 0.0);
}

TEST_F(LdlTest, VectorPermutationOption)
{
    // 'vector' opt: P returned as 1×n row of indices.
    eval("[L, D, p] = ldl([4 2; 2 5], 'vector');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(p, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(p, 2)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("p(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("p(2)"), 2.0);
}

// ── Upper triangular form ─────────────────────────────────────────────
TEST_F(LdlTest, UpperFormReturnsTransposedL)
{
    // 'upper' opt: returns U (unit upper) such that A = U'*D*U.
    eval("A = [4 2 1; 2 5 3; 1 3 6];"
         "[U, D] = ldl(A, 'upper');"
         "res = max(max(abs(A - U'*D*U)));");
    EXPECT_LT(evalScalar("res"), 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("U(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("U(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("U(3,3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("U(1,2)"), 0.5);  // upper entries non-zero
    // Lower triangle is zero.
    EXPECT_DOUBLE_EQ(evalScalar("U(2,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("U(3,1)"), 0.0);
}

// ── Indefinite case (works without pivoting) ──────────────────────────
TEST_F(LdlTest, IndefiniteWithoutPivoting)
{
    eval("B = [2 -1; -1 -3];"
         "[L, D] = ldl(B);");
    EXPECT_DOUBLE_EQ(evalScalar("L(1,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(2,1)"), -0.5);
    EXPECT_DOUBLE_EQ(evalScalar("D(1,1)"),  2.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(2,2)"), -3.5);  // negative pivot OK
    eval("res = max(max(abs(B - L*D*L')));");
    EXPECT_LT(evalScalar("res"), 1e-12);
}

// ── 1-output form ─────────────────────────────────────────────────────
TEST_F(LdlTest, SingleOutputReturnsLOnly)
{
    eval("L = ldl([4 2; 2 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("L(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(2,1)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("L(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(1,2)"), 0.0);
}

// ── Larger PD residual test ───────────────────────────────────────────
TEST_F(LdlTest, FiveByFivePositiveDefinite)
{
    eval("A = magic(5); A = A + A'; A = A + 50*eye(5);"  // SPD
         "[L, D] = ldl(A);"
         "res = max(max(abs(A - L*D*L')));");
    EXPECT_LT(evalScalar("res"), 1e-10);
    // All diagonal of D should be > 0 for a PD matrix.
    EXPECT_GT(evalScalar("D(1,1)"), 0.0);
    EXPECT_GT(evalScalar("D(5,5)"), 0.0);
}

// ── Edge cases ────────────────────────────────────────────────────────
TEST_F(LdlTest, OneByOneScalar)
{
    eval("[L, D] = ldl([7.0]);");
    EXPECT_DOUBLE_EQ(evalScalar("L(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(1,1)"), 7.0);
}

TEST_F(LdlTest, EmptyInput)
{
    eval("[L, D] = ldl(zeros(0, 0));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(L, 1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(D, 1)")), 0);
}

// ── Error paths ───────────────────────────────────────────────────────
TEST_F(LdlTest, NonSquareThrows)
{
    bool threw = false;
    try { eval("ldl([1 2 3; 4 5 6]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(LdlTest, BadOptionThrows)
{
    bool threw = false;
    try { eval("ldl([1 2; 2 4], 'bogus');"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
