// libs/builtin/tests/eig_test.cpp
//
// Regression guard for symmetric eig (Phase 2 -- Jacobi rotations).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EigTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(EigTest, EigDiagonalIsItself)
{
    eval("e = eig(diag([3 7 1 9 5]));");
    // Sorted ascending.
    EXPECT_DOUBLE_EQ(evalScalar("e(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(3)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(4)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(5)"), 9.0);
}

TEST_F(EigTest, EigIdentityIsAllOnes)
{
    eval("e = eig(eye(4));");
    EXPECT_DOUBLE_EQ(evalScalar("e(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(e)"), 4.0);
}

TEST_F(EigTest, EigSymmetricGeneralCaseIdentity)
{
    eval("A = [4 1 2; 1 3 0; 2 0 5]; [V, D] = eig(A);");
    // A*V == V*D.
    EXPECT_NEAR(evalScalar("max(max(abs(A*V - V*D)))"), 0.0, 1e-12);
    // V orthogonal.
    EXPECT_NEAR(evalScalar("max(max(abs(V'*V - eye(3))))"), 0.0, 1e-12);
}

TEST_F(EigTest, EigSumEqualsTrace)
{
    // Sum of eigenvalues == trace(A).
    eval("A = [4 1 2; 1 3 0; 2 0 5]; e = eig(A);");
    EXPECT_NEAR(evalScalar("sum(e) - trace(A)"), 0.0, 1e-12);
}

TEST_F(EigTest, EigProductEqualsDet)
{
    // Product of eigenvalues == det(A) for symmetric A.
    eval("A = [4 1 2; 1 3 0; 2 0 5]; e = eig(A);");
    EXPECT_NEAR(evalScalar("prod(e) - det(A)"), 0.0, 1e-10);
}

TEST_F(EigTest, EigAsymmetricSingleOutputWorks)
{
    // Phase 2c-2: e = eig(A) for asymmetric A via Souriau-Faddeev + roots.
    // Companion matrix of x^3 - 7x + 6: roots = 1, 2, -3.
    eval("A = [0 7 -6; 1 0 0; 0 1 0]; e = sort(real(eig(A)));");
    EXPECT_NEAR(evalScalar("e(1)"), -3.0, 1e-10);
    EXPECT_NEAR(evalScalar("e(2)"),  1.0, 1e-10);
    EXPECT_NEAR(evalScalar("e(3)"),  2.0, 1e-10);
}

TEST_F(EigTest, EigAsymmetricComplexEigenvalues)
{
    // Rotation: eig([0 -1; 1 0]) = ±i.
    eval("e = eig([0 -1; 1 0]);");
    EXPECT_TRUE(eval("any(abs(real(e)) < 1e-12)").toBool());
    EXPECT_TRUE(eval("any(abs(imag(e)) > 0.5)").toBool());
}

TEST_F(EigTest, EigAsymmetricVDForRealEigvalsWorks)
{
    // After Phase 2c-3a: [V, D] for asymmetric A WITH REAL eigvals
    // works via null space. [1 2; 3 4] has eigvals (5±sqrt(33))/2,
    // both real. Should succeed and reconstruct A*V == V*D.
    eval("A = [1 2; 3 4]; [V, D] = eig(A);");
    EXPECT_NEAR(evalScalar("max(max(abs(A*V - V*D)))"), 0.0, 1e-10);
}

TEST_F(EigTest, EigAsymmetricVDForComplexEigvalsThrows)
{
    // Phase 2c-3a does NOT support complex eigenvectors -- throws cleanly.
    EXPECT_THROW(eval("[V, D] = eig([0 -1; 1 0]);"), std::exception);
}

TEST_F(EigTest, EigNonSquareRejected)
{
    EXPECT_THROW(eval("eig([1 2 3; 4 5 6]);"), std::exception);
}

TEST_F(EigTest, EigPositiveDefinite)
{
    // SPD matrix: all eigenvalues > 0.
    eval("S = [4 12 -16; 12 37 -43; -16 -43 98]; e = eig(S);");
    EXPECT_GT(evalScalar("min(e)"), 0.0);
}

// ── 'vector' / 'matrix' options + generalized eig(A,B) ──────────

TEST_F(EigTest, EigVectorOptionIsColumnVector)
{
    eval("A = [2 0 0; 0 3 0; 0 0 5]; e = eig(A, 'vector');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(e, 2)")), 1);   // column
    EXPECT_EQ(static_cast<int>(evalScalar("numel(e)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("e(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(3)"), 5.0);
}

TEST_F(EigTest, EigMatrixOptionIsDiagonal)
{
    eval("A = [2 0 0; 0 3 0; 0 0 5]; D = eig(A, 'matrix');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(D, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(D, 2)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("D(1, 1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(3, 3)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(1, 2)"), 0.0);              // off-diagonal zero
}

TEST_F(EigTest, EigGeneralizedDiagonal)
{
    // eig(A,B) = eigenvalues of B\A. A=diag(2,3,5), B=diag(1,2,1).
    eval("A = [2 0 0; 0 3 0; 0 0 5]; B = [1 0 0; 0 2 0; 0 0 1];"
         "e = sort(eig(A, B));");
    EXPECT_NEAR(evalScalar("e(1)"), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("e(2)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("e(3)"), 5.0, 1e-12);
}

TEST_F(EigTest, EigGeneralizedSymmetricPairReal)
{
    // Symmetric-definite pair: real eigenvalues matching MATLAB R2025b.
    eval("A = [4 1; 1 3]; B = [2 0; 0 1]; e = sort(eig(A, B));");
    EXPECT_NEAR(evalScalar("e(1)"), 1.6339745962155614, 1e-10);
    EXPECT_NEAR(evalScalar("e(2)"), 3.3660254037844384, 1e-10);
}

TEST_F(EigTest, EigGeneralizedVDReconstructs)
{
    // [V,D] = eig(A,B) satisfies A*V = B*V*D.
    eval("A = [4 1; 1 3]; B = [2 0; 0 1]; [V, D] = eig(A, B);");
    EXPECT_NEAR(evalScalar("max(max(abs(A*V - B*V*D)))"), 0.0, 1e-10);
}

TEST_F(EigTest, EigUnknownOptionThrows)
{
    EXPECT_THROW(eval("eig([1 2; 3 4], 'bogus');"), std::exception);
}
