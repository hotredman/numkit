// libs/linalg/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug in bugs/linalg/*.md. Disabled until
// fixed; remove `DISABLED_` to turn into a live regression guard.
// MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class LinalgKnownBug : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// bugs/linalg/qr-pivoting.md — column-pivoting QR, 3rd output P. FIXED
// 2026-06-05 (deep coverage in libs/linalg/tests/qr_pivoting_test.cpp).
TEST_F(LinalgKnownBug, QrColumnPivoting)
{
    eval("A = [1 2; 3 4; 5 6]; [Q, R, P] = qr(A);");
    // A*P == Q*R, columns ordered by decreasing norm -> P swaps the columns.
    EXPECT_LT(evalScalar("max(max(abs(A*P - Q*R)))"), 1e-10);
    EXPECT_NEAR(evalScalar("abs(R(1,1))"), 7.483315, 1e-5);
    eval("[Q2, R2, p2] = qr(A, 'vector');");
    EXPECT_DOUBLE_EQ(evalScalar("p2(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("p2(2)"), 1.0);
}

// bugs/linalg/eig-left-vectors.md — 3rd output W (left eigenvectors). FIXED
// 2026-06-05 (deep coverage in libs/linalg/tests/eig_left_vectors_test.cpp).
TEST_F(LinalgKnownBug, EigLeftVectors)
{
    eval("A = [4 -2; 1 1]; [V, D, W] = eig(A);");
    // Left eigenvectors satisfy W'*A = D*W'.
    EXPECT_LT(evalScalar("max(max(abs(W'*A - D*W')))"), 1e-10);
}

// bugs/linalg/norm-complex.md — FIXED (norm of a complex array by magnitude).
// Live regression guard moved to libs/linalg/tests/norm_complex_test.cpp.

// bugs/linalg/kron-integer-class.md — kron of integer operands kept the
// integer class (saturating). FIXED 2026-06-05; deep coverage in
// libs/linalg/tests/kron_integer_class_test.cpp.
TEST_F(LinalgKnownBug, KronIntegerClass)
{
    eval("k = kron(int8([1 2]), int8([1 1]));");
    EXPECT_TRUE(eval("isa(k, 'int8')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("double(k(3))"), 2.0);
    // Saturating product: int8(100)*int8(2) = 200 -> 127.
    EXPECT_DOUBLE_EQ(evalScalar("double(kron(int8(100), int8(2)))"), 127.0);
    // int + scalar double keeps the integer class (scalar cast, round-away).
    EXPECT_TRUE(eval("isa(kron(int8([2 3]), 2), 'int8')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("double(kron(int8(2), 1.5))"), 3.0);
    // double*double stays double.
    EXPECT_TRUE(eval("isa(kron([1 2], [3 4]), 'double')").toBool());
}

// bugs/linalg/complex-matrix-unsupported.md — complex matrix linear algebra.
TEST_F(LinalgKnownBug, DISABLED_ComplexMatrixOps)
{
    eval("B = [1+1i 2; 3 4-1i];");
    eval("t = trace(B);");          // MATLAB: 5+0i
    EXPECT_NEAR(evalScalar("real(t)"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(t)"), 0.0, 1e-12);
    eval("d = det(B);");            // MATLAB: -1+3i
    EXPECT_NEAR(evalScalar("real(d)"), -1.0, 1e-10);
    EXPECT_NEAR(evalScalar("imag(d)"),  3.0, 1e-10);
}

// bugs/linalg/funm.md — general matrix function funm(A, fun).
TEST_F(LinalgKnownBug, DISABLED_Funm)
{
    eval("F = funm([2 0; 0 3], @exp);");   // MATLAB: diag(e^2, e^3)
    EXPECT_NEAR(evalScalar("F(1,1)"), 7.38905609893065, 1e-9);
    EXPECT_NEAR(evalScalar("F(2,2)"), 20.0855369231877, 1e-9);
    EXPECT_NEAR(evalScalar("F(1,2)"), 0.0, 1e-12);
}

// bugs/linalg/qz-gsvd.md — generalized Schur + generalized SVD.
TEST_F(LinalgKnownBug, DISABLED_QzGsvd)
{
    eval("A = [1 2; 3 4]; B = [1 0; 0 1]; [AA, BB, Q, Z] = qz(A, B);");
    EXPECT_LT(evalScalar("max(max(abs(Q*A*Z - AA)))"), 1e-10);   // reconstruction
    EXPECT_LT(evalScalar("max(max(abs(Q*B*Z - BB)))"), 1e-10);
    eval("S = sort(gsvd([1 2; 3 4], [1 0; 0 1]));");
    EXPECT_NEAR(evalScalar("S(1)"), 0.365966190626, 1e-6);
    EXPECT_NEAR(evalScalar("S(2)"), 5.46498570422,  1e-6);
}

// bugs/linalg/schur-nonsymmetric.md — real Schur form of a non-symmetric matrix.
TEST_F(LinalgKnownBug, DISABLED_SchurNonsymmetric)
{
    eval("A = [2 1; 0 3]; [U, T] = schur(A);");
    EXPECT_LT(evalScalar("max(max(abs(U*T*U' - A)))"),    1e-10);  // reconstruction
    EXPECT_LT(evalScalar("max(max(abs(U'*U - eye(2))))"), 1e-10);  // U orthogonal
    eval("d = sort(diag(T));");
    EXPECT_NEAR(evalScalar("d(1)"), 2.0, 1e-9);
    EXPECT_NEAR(evalScalar("d(2)"), 3.0, 1e-9);
}
