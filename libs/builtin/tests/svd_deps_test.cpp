// libs/builtin/tests/svd_deps_test.cpp
//
// Regression guard for the 6 SVD-dependent functions:
//   rank, pinv, cond, orth, null, normest

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SvdDepsTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── rank ───────────────────────────────────────────────────

TEST_F(SvdDepsTest, RankFullRankSquare)
{
    EXPECT_EQ(static_cast<int>(evalScalar("rank([1 2 3; 4 5 6; 7 8 10])")), 3);
}

TEST_F(SvdDepsTest, RankIdentityIsN)
{
    EXPECT_EQ(static_cast<int>(evalScalar("rank(eye(7))")), 7);
}

TEST_F(SvdDepsTest, RankRankDeficient)
{
    // [1 2; 2 4; 3 6] -- columns are multiples, rank 1.
    EXPECT_EQ(static_cast<int>(evalScalar("rank([1 2; 2 4; 3 6])")), 1);
}

TEST_F(SvdDepsTest, RankZeroMatrix)
{
    EXPECT_EQ(static_cast<int>(evalScalar("rank(zeros(4))")), 0);
}

// ── pinv ───────────────────────────────────────────────────

TEST_F(SvdDepsTest, PinvFullRankSquareEqualsInv)
{
    eval("A = [4 7; 2 6]; P = pinv(A);");
    // For invertible A, pinv == inv.
    EXPECT_NEAR(evalScalar("max(max(abs(P - inv(A))))"), 0.0, 1e-12);
}

TEST_F(SvdDepsTest, PinvMoorePenroseProperties)
{
    // Moore-Penrose: A*P*A == A and P*A*P == P.
    eval("A = [1 2; 3 4; 5 6]; P = pinv(A);");
    EXPECT_NEAR(evalScalar("max(max(abs(A*P*A - A)))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("max(max(abs(P*A*P - P)))"), 0.0, 1e-12);
}

TEST_F(SvdDepsTest, PinvShape)
{
    // pinv(m×n) is n×m.
    eval("P = pinv([1 2 3; 4 5 6]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,2)")), 2);
}

// ── cond ───────────────────────────────────────────────────

TEST_F(SvdDepsTest, CondIdentityIsOne)
{
    EXPECT_DOUBLE_EQ(evalScalar("cond(eye(5))"), 1.0);
}

TEST_F(SvdDepsTest, CondHilbertIsLarge)
{
    // Hilbert matrices are notoriously ill-conditioned.
    EXPECT_GT(evalScalar("cond(hilb(8))"), 1e9);
}

TEST_F(SvdDepsTest, CondSingularIsInf)
{
    EXPECT_TRUE(std::isinf(evalScalar("cond([1 2; 2 4])")));
}

// ── orth ───────────────────────────────────────────────────

TEST_F(SvdDepsTest, OrthFullRankSquare)
{
    eval("A = [1 2 3; 4 5 6; 7 8 10]; Q = orth(A);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(Q,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(Q,2)")), 3);
    EXPECT_NEAR(evalScalar("max(max(abs(Q'*Q - eye(3))))"), 0.0, 1e-12);
}

TEST_F(SvdDepsTest, OrthRankDeficient)
{
    // rank-1 -> orth gives 1 column.
    eval("A = [1 2; 2 4; 3 6]; Q = orth(A);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(Q,2)")), 1);
}

// ── null ───────────────────────────────────────────────────

TEST_F(SvdDepsTest, NullFullRankIsEmpty)
{
    eval("N = null([1 2 3; 4 5 6; 7 8 10]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(N,2)")), 0);
}

TEST_F(SvdDepsTest, NullRankDeficientGivesBasis)
{
    // rank-1, 2 columns -> null space is 1-dimensional.
    eval("A = [1 2; 2 4; 3 6]; N = null(A);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(N,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(N,2)")), 1);
    // A*N must be zero.
    EXPECT_NEAR(evalScalar("max(abs(A*N))"), 0.0, 1e-12);
}

// ── normest ────────────────────────────────────────────────

TEST_F(SvdDepsTest, NormestEqualsLargestSingular)
{
    eval("A = [1 2 3; 4 5 6; 7 8 10]; n = normest(A); s = svd(A);");
    EXPECT_NEAR(evalScalar("n - s(1)"), 0.0, 1e-12);
}

TEST_F(SvdDepsTest, NormestIdentityIsOne)
{
    EXPECT_DOUBLE_EQ(evalScalar("normest(eye(5))"), 1.0);
}
