// libs/builtin/tests/balance_test.cpp
//
// Regression guard for balance — Parlett-Reinsch diagonal scaling
// (linalg cycle 5).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class BalanceTest : public ::testing::Test
{
public:
    Engine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Similarity invariance ──────────────────────────────────────────────
// B = inv(T) * A * T  for all inputs. This is the defining property.
TEST_F(BalanceTest, SimilarityHoldsForGenericMatrix)
{
    eval("A = [1 100 10000; 0.01 1 100; 0.0001 0.01 1];"
         "[T, B] = balance(A);"
         "res = max(max(abs(B - T\\A*T)));");
    EXPECT_LT(evalScalar("res"), 1e-12);
}

TEST_F(BalanceTest, SimilarityHoldsForExtremeRange2x2)
{
    eval("A = [1 1e6; 1e-6 1];"
         "[T, B] = balance(A);"
         "res = max(max(abs(B - T\\A*T)));");
    EXPECT_LT(evalScalar("res"), 1e-12);
}

// ── Eigenvalue preservation ────────────────────────────────────────────
TEST_F(BalanceTest, EigenvaluesPreserved)
{
    eval("A = [1 100 10000; 0.01 1 100; 0.0001 0.01 1];"
         "[~, B] = balance(A);"
         "ediff = max(abs(sort(real(eig(A))) - sort(real(eig(B)))));");
    EXPECT_LT(evalScalar("ediff"), 1e-9);
}

// ── Already-balanced inputs are unchanged ──────────────────────────────
TEST_F(BalanceTest, IdentityIsFixedPoint)
{
    eval("[T, B] = balance(eye(3));"
         "diff_T = max(max(abs(T - eye(3))));"
         "diff_B = max(max(abs(B - eye(3))));");
    EXPECT_DOUBLE_EQ(evalScalar("diff_T"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("diff_B"), 0.0);
}

TEST_F(BalanceTest, DiagonalIsFixedPoint)
{
    eval("[T, B] = balance(diag([1 2 3]));"
         "diff_T = max(max(abs(T - eye(3))));"
         "diff_B = max(max(abs(B - diag([1 2 3])))) ;");
    EXPECT_DOUBLE_EQ(evalScalar("diff_T"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("diff_B"), 0.0);
}

// ── Output forms ───────────────────────────────────────────────────────
TEST_F(BalanceTest, SingleOutputReturnsB)
{
    eval("A = [1 100 10000; 0.01 1 100; 0.0001 0.01 1];"
         "B1 = balance(A);"
         "[~, B2] = balance(A);"
         "diff = max(max(abs(B1 - B2)));");
    EXPECT_DOUBLE_EQ(evalScalar("diff"), 0.0);
}

TEST_F(BalanceTest, ThreeOutputForm)
{
    // [S, P, B] form: S = column of scalings, P = perm column, B = balanced.
    eval("A = [1 100 10000; 0.01 1 100; 0.0001 0.01 1];"
         "[S, P, B] = balance(A);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(S, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(S, 2)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(P, 1)")), 3);
    // P is identity in v1 (no permutation phase).
    EXPECT_DOUBLE_EQ(evalScalar("P(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(3)"), 3.0);
    // S entries should be positive (scaling factors).
    EXPECT_GT(evalScalar("S(1)"), 0.0);
    EXPECT_GT(evalScalar("S(2)"), 0.0);
    EXPECT_GT(evalScalar("S(3)"), 0.0);
}

TEST_F(BalanceTest, NopermOption)
{
    // 'noperm' should give same scaling result as default in v1.
    eval("A = [1 100 10000; 0.01 1 100; 0.0001 0.01 1];"
         "[T1, B1] = balance(A);"
         "[T2, B2] = balance(A, 'noperm');"
         "dT = max(max(abs(T1 - T2)));"
         "dB = max(max(abs(B1 - B2)));");
    EXPECT_DOUBLE_EQ(evalScalar("dT"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("dB"), 0.0);
}

// ── Balanced output has small inf-norm range ──────────────────────────
TEST_F(BalanceTest, BalancingReducesNormRange)
{
    // Original A has wildly different row sums; B should have closer
    // row/col norms.
    eval("A = [1 100 10000; 0.01 1 100; 0.0001 0.01 1];"
         "[~, B] = balance(A);"
         "rB = sum(abs(B), 2);"
         "spread = max(rB) / min(rB);");
    EXPECT_LT(evalScalar("spread"), 10.0);  // much better than original
}

// ── Edge cases ────────────────────────────────────────────────────────
TEST_F(BalanceTest, OneByOneScalar)
{
    eval("[T, B] = balance([5.0]);");
    EXPECT_DOUBLE_EQ(evalScalar("T(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"), 5.0);
}

TEST_F(BalanceTest, EmptyInput)
{
    eval("[T, B] = balance(zeros(0, 0));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 0);
}

// ── Error paths ───────────────────────────────────────────────────────
TEST_F(BalanceTest, NonSquareThrows)
{
    bool threw = false;
    try { eval("balance([1 2 3; 4 5 6]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(BalanceTest, BadOptionThrows)
{
    bool threw = false;
    try { eval("balance([1 2; 3 4], 'wrong');"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
