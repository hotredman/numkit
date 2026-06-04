// libs/linalg/tests/norm_complex_test.cpp
//
// Regression guard for bugs/linalg/norm-complex.md (FIXED): norm() now norms
// a COMPLEX array by element magnitude (vector 1/2/Inf/p and matrix 1/Inf/
// 'fro'), matching MATLAB. The complex matrix 2-norm (spectral) still needs a
// complex SVD and is expected to throw. MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class NormComplexTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Complex vector norms: |3+4i|=5, |0|=0, |-2i|=2.
TEST_F(NormComplexTest, VectorNorms)
{
    eval("v = [3+4i 0 -2i];");
    EXPECT_NEAR(evalScalar("norm(v)"),       5.385164807134504, 1e-12);  // sqrt(29)
    EXPECT_NEAR(evalScalar("norm(v, 2)"),    5.385164807134504, 1e-12);
    EXPECT_NEAR(evalScalar("norm(v, 1)"),    7.0,               1e-12);
    EXPECT_NEAR(evalScalar("norm(v, Inf)"),  5.0,               1e-12);
    EXPECT_NEAR(evalScalar("norm(v, 'fro')"),5.385164807134504, 1e-12);
    EXPECT_NEAR(evalScalar("norm(v, 3)"),    5.104468722, 1e-6);          // (125+8)^(1/3)
}

// A complex scalar norms to its magnitude.
TEST_F(NormComplexTest, ScalarMagnitude)
{
    EXPECT_NEAR(evalScalar("norm(3+4i)"), 5.0, 1e-12);
}

// Complex matrix 1-norm (max abs col sum) and Inf-norm (max abs row sum).
TEST_F(NormComplexTest, MatrixOneInf)
{
    eval("M = [1+1i 2; 3 4-1i];");
    EXPECT_NEAR(evalScalar("norm(M, 1)"),   6.123105625617661, 1e-12);  // 2+sqrt(17)
    EXPECT_NEAR(evalScalar("norm(M, Inf)"), 7.123105625617661, 1e-12);  // 3+sqrt(17)
}

// Complex Frobenius norm: sqrt(sum |z|^2) = sqrt(2+9+4+17) = sqrt(32).
TEST_F(NormComplexTest, MatrixFrobenius)
{
    eval("M = [1+1i 2; 3 4-1i];");
    EXPECT_NEAR(evalScalar("norm(M, 'fro')"), 5.656854249492380, 1e-12);
}

// Complex matrix 2-norm (spectral) still requires a complex SVD -> throws.
TEST_F(NormComplexTest, MatrixSpectralStillThrows)
{
    EXPECT_ANY_THROW(eval("norm([1+1i 2; 3 4-1i], 2);"));
}

// Real-input norms must be unaffected by the complex branch.
TEST_F(NormComplexTest, RealUnchanged)
{
    EXPECT_NEAR(evalScalar("norm([3 4])"),        5.0,               1e-12);
    EXPECT_NEAR(evalScalar("norm([1 2; 3 4])"),   5.464985704219043, 1e-12);
    EXPECT_NEAR(evalScalar("norm([1 2; 3 4],'fro')"), 5.477225575051661, 1e-12);
}
