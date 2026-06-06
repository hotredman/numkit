// libs/linalg/tests/eig_left_vectors_test.cpp
//
// Regression guard for bugs/linalg/eig-left-vectors.md (FIXED): [V,D,W]=eig(A)
// returns the left eigenvectors W (columns; W'*A = D*W'), the right
// eigenvectors of A' reordered to D's eigenvalue order and normalized to unit
// 2-norm (MATLAB). For symmetric A, W == V. W column signs may differ from
// MATLAB (eig sign convention), so validation is sign-agnostic: the defining
// relation, unit-norm columns, abs() / sum(abs()). MATLAB R2025b reference.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class EigLeftVectorsTest : public ::testing::Test
{
public:
    numkit::StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Non-symmetric 2x2: W'*A = D*W', unit-norm columns, sum(abs(W)) matches MATLAB
// (order/sign independent).
TEST_F(EigLeftVectorsTest, NonSymmetric2x2)
{
    eval("A = [4 -2; 1 1]; [V, D, W] = eig(A);");
    EXPECT_LT(evalScalar("max(max(abs(W'*A - D*W')))"), 1e-10);
    EXPECT_NEAR(evalScalar("norm(W(:,1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("norm(W(:,2))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sum(sum(abs(W)))"), 2.755854, 1e-5);
}

// Symmetric A: W == V (left == right eigenvectors).
TEST_F(EigLeftVectorsTest, SymmetricWEqualsV)
{
    eval("B = [2 1; 1 3]; [V, D, W] = eig(B);");
    EXPECT_LT(evalScalar("max(max(abs(W - V)))"), 1e-10);
    EXPECT_LT(evalScalar("max(max(abs(W'*B - D*W')))"), 1e-10);
    EXPECT_NEAR(evalScalar("abs(W(1,1))"), 0.850651, 1e-5);   // sign-agnostic
}

// 3x3 symmetric: relation + W == V.
TEST_F(EigLeftVectorsTest, Symmetric3x3)
{
    eval("C = [6 2 1; 2 3 1; 1 1 1]; [V, D, W] = eig(C);");
    EXPECT_LT(evalScalar("max(max(abs(W'*C - D*W')))"), 1e-10);
    EXPECT_LT(evalScalar("max(max(abs(W - V)))"), 1e-10);
}

// 3x3 non-symmetric (real eigenvalues): relation + sum(abs(W)) matches MATLAB.
TEST_F(EigLeftVectorsTest, NonSymmetric3x3)
{
    eval("N = [2 0 0; 1 3 0; 0 1 4]; [V, D, W] = eig(N);");
    EXPECT_LT(evalScalar("max(max(abs(W'*N - D*W')))"), 1e-10);
    EXPECT_NEAR(evalScalar("sum(sum(abs(W)))"), 4.080880, 1e-5);
    // every W column is unit norm
    EXPECT_NEAR(evalScalar("norm(W(:,1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("norm(W(:,3))"), 1.0, 1e-12);
}

// Left/right eigenvectors of different eigenvalues are orthogonal → W'*V is
// diagonal (off-diagonal ≈ 0).
TEST_F(EigLeftVectorsTest, BiorthogonalStructure)
{
    eval("N = [2 0 0; 1 3 0; 0 1 4]; [V, D, W] = eig(N); G = W'*V;");
    EXPECT_LT(evalScalar("abs(G(1,2))"), 1e-10);
    EXPECT_LT(evalScalar("abs(G(1,3))"), 1e-10);
    EXPECT_LT(evalScalar("abs(G(2,3))"), 1e-10);
}

// The 2-output form is unchanged.
TEST_F(EigLeftVectorsTest, TwoOutputUnchanged)
{
    eval("B = [2 1; 1 3]; [V, D] = eig(B);");
    EXPECT_LT(evalScalar("max(max(abs(B*V - V*D)))"), 1e-10);
}
