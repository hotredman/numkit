// libs/builtin/tests/svd_test.cpp
//
// Regression guard for builtin::svd (Phase 1 -- one-sided Jacobi).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SvdTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Singular values ────────────────────────────────────────

TEST_F(SvdTest, SvdDiagonal)
{
    // Singular values of a diagonal matrix are abs(diagonal), descending.
    eval("s = svd(diag([3 1 2]));");
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(3)"), 1.0);
}

TEST_F(SvdTest, SvdIdentity)
{
    eval("s = svd(eye(5));");
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(5)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(s)"), 5.0);
}

TEST_F(SvdTest, SvdValuesDescending)
{
    eval("s = svd([1 2 3; 4 5 6; 7 8 10]);");
    // Descending order.
    EXPECT_GT(evalScalar("s(1)"), evalScalar("s(2)"));
    EXPECT_GT(evalScalar("s(2)"), evalScalar("s(3)"));
    EXPECT_GT(evalScalar("s(3)"), 0.0);
}

// ── Full decomposition ─────────────────────────────────────

TEST_F(SvdTest, SvdReconstructionSquare)
{
    eval("A = [1 2 3; 4 5 6; 7 8 10]; [U, S, V] = svd(A);");
    EXPECT_NEAR(evalScalar("max(max(abs(U*S*V' - A)))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("max(max(abs(U'*U - eye(3))))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("max(max(abs(V'*V - eye(3))))"), 0.0, 1e-12);
}

TEST_F(SvdTest, SvdShapeTall)
{
    // 4×3 input: U is 4×4, S is 4×3, V is 3×3.
    eval("A = [1 2 3; 4 5 6; 7 8 10; 1 0 1]; [U, S, V] = svd(A);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(U,1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(U,2)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(S,1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(S,2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(V,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(V,2)")), 3);
    EXPECT_NEAR(evalScalar("max(max(abs(U*S*V' - A)))"), 0.0, 1e-12);
}

TEST_F(SvdTest, SvdShapeWide)
{
    // 3×4 input: U is 3×3, S is 3×4, V is 4×4.
    eval("A = [1 2 3 4; 5 6 7 8; 9 10 11 13]; [U, S, V] = svd(A);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(U,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(V,1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(V,2)")), 4);
    EXPECT_NEAR(evalScalar("max(max(abs(U*S*V' - A)))"), 0.0, 1e-12);
}

TEST_F(SvdTest, SvdRankDeficient)
{
    // Rank-1 matrix: only one non-zero singular value.
    eval("A = [1 2; 2 4; 3 6]; s = svd(A);");
    EXPECT_GT(evalScalar("s(1)"), 1.0);
    EXPECT_NEAR(evalScalar("s(2)"), 0.0, 1e-12);
}

TEST_F(SvdTest, SvdScalar)
{
    // 1×1 matrix: singular value is abs(value).
    eval("s = svd(7.5);");
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 7.5);
}

TEST_F(SvdTest, SvdEconTall)
{
    // 3x2 tall: economy → U 3x2, S 2x2, V 2x2, U*S*V' == A.
    eval("A = [1 2; 3 4; 5 6]; [U, S, V] = svd(A, 'econ');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(U,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(U,2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(S,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(S,2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(V,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(V,2)")), 2);
    EXPECT_NEAR(evalScalar("S(1,1)"), 9.5255180915651074, 1e-9);
    EXPECT_NEAR(evalScalar("S(2,2)"), 0.5143005806586447, 1e-9);
    EXPECT_NEAR(evalScalar("max(max(abs(U*S*V' - A)))"), 0.0, 1e-12);
}

TEST_F(SvdTest, SvdEconLegacyZero)
{
    // svd(A, 0) is the legacy spelling of 'econ'.
    eval("A = [1 2; 3 4; 5 6]; [U, S, V] = svd(A, 0);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(U,2)")), 2);
    EXPECT_NEAR(evalScalar("max(max(abs(U*S*V' - A)))"), 0.0, 1e-12);
}
