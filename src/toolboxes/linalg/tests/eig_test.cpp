// toolboxes/builtin/tests/eig_test.cpp
//
// Regression guard for symmetric eig (Phase 2 -- Jacobi rotations).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EigTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
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

TEST_F(EigTest, EigAsymmetricVDForComplexEigvalsWorks)
{
    // Phase 1.4 supports complex eigenvectors via complex Schur decomposition.
    eval("A = [0 -1; 1 0]; [V, D] = eig(A);");
    EXPECT_NEAR(evalScalar("max(max(abs(A*V - V*D)))"), 0.0, 1e-10);
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

// --- AHP Perron selection (bugs/opened/linalg/eig-vector-selection-ahp.md) ---
//
// Real-world AHP code selects the dominant eigenvector by
// [~, idx] = max(diag(D)); w = V(:, idx). The engines return eig columns
// in DIFFERENT orders (numkit idx=3, MATLAB idx=1 for this matrix — both
// legal, MATLAB guarantees no order), so only the selection idiom may be
// relied upon. Pins it for a perfectly consistent matrix (degenerate
// zero-eigenvalue space): the selected vector must be the Perron weights.

TEST_F(EigTest, EigAHPConsistentPerronSelection)
{
    eval("A = [1 1 5/3; 1 1 5/3; 3/5 3/5 1];");
    eval("[V, D] = eig(A); [mm, idx] = max(diag(D)); w1 = V(:, idx); w1 = w1 / sum(w1);");
    EXPECT_NEAR(evalScalar("mm"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("w1(1)"), 5.0 / 13.0, 1e-12);
    EXPECT_NEAR(evalScalar("w1(2)"), 5.0 / 13.0, 1e-12);
    EXPECT_NEAR(evalScalar("w1(3)"), 3.0 / 13.0, 1e-12);
}

// --- bugs/closed/linalg/eig-vector-selection-ahp.md (FIXED: bulge-chase column) ---
// Real 3x3 reciprocal matrix whose complex-Schur QR used to hit the
// iteration budget and return a garbage spectrum ({1.54+3.66i, ...}).
// The bulge-chasing Givens read h(k+1, k) instead of the bulge position
// h(k+1, k-1); after the fix the [V,D] path matches MATLAB R2025b:
// spectrum {4.23118, -0.61559, -0.61559+/-2.19782i}, Perron weight of the
// dominant eigenvalue = [0.730911 0.588371 0.346858] (17-digit probed).
TEST_F(EigTest, EigAHPReciprocal3x3Spectrum)
{
    eval("A = [1 9 3; 0.1111111111111111 1 8; 0.3333333333333333 0.125 1];");
    eval("[V, D] = eig(A);");
    // Spectrum as a SET (column order is engine-defined).
    eval("lams = sortrows([real(diag(D)), imag(diag(D))], [1 2]);");
    EXPECT_NEAR(evalScalar("lams(1,1)"), -0.615589888895067, 1e-9);
    EXPECT_NEAR(evalScalar("lams(1,2)"), -2.197815294172810, 1e-9);
    EXPECT_NEAR(evalScalar("lams(2,1)"), -0.615589888895067, 1e-9);
    EXPECT_NEAR(evalScalar("lams(2,2)"),  2.197815294172810, 1e-9);
    EXPECT_NEAR(evalScalar("lams(3,1)"),  4.231179777790134, 1e-9);
    EXPECT_NEAR(evalScalar("lams(3,2)"),  0.0, 1e-9);
    // A·V == V·D (eigenvector residual — the AHP symptom was a basis-vector
    // "eigenvector" that satisfied nothing).
    EXPECT_NEAR(evalScalar("max(max(abs(A*V - V*D)))"), 0.0, 1e-9);
    // The Perron weight via the real-code selection idiom.
    eval("[~, i] = max(real(diag(D))); w = V(:, i); w = w / sum(w);");
    eval("wr = real(w);");
    EXPECT_NEAR(evalScalar("wr(1)"), 0.696349678003404, 1e-9);
    EXPECT_NEAR(evalScalar("wr(2)"), 0.223180005307580, 1e-9);
    EXPECT_NEAR(evalScalar("wr(3)"), 0.080470316689016, 1e-9);
}

// --- bugs/opened/linalg/eig-complex-input-rejected.md ---
// Complex INPUT matrices must decompose like MATLAB does, not throw
// "Not a double array".
TEST_F(EigTest, DISABLED_EigComplexInput)
{
    eval("[V, D] = eig(complex([1 2; 3 4]));");
    eval("l = sort(real(diag(D)));");
    EXPECT_NEAR(evalScalar("l(1)"), -0.372281323269014, 1e-9);
    EXPECT_NEAR(evalScalar("l(2)"),  5.372281323269014, 1e-9);
}
