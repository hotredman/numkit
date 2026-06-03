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

// bugs/linalg/qr-pivoting.md — column-pivoting QR, 3rd output P.
TEST_F(LinalgKnownBug, DISABLED_QrColumnPivoting)
{
    eval("A = [1 2; 3 4; 5 6]; [Q, R, P] = qr(A);");
    // A*P == Q*R, columns ordered by decreasing norm -> P swaps the columns.
    EXPECT_LT(evalScalar("max(max(abs(A*P - Q*R)))"), 1e-10);
    EXPECT_NEAR(evalScalar("abs(R(1,1))"), 7.483315, 1e-5);
    eval("[Q2, R2, p2] = qr(A, 'vector');");
    EXPECT_DOUBLE_EQ(evalScalar("p2(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("p2(2)"), 1.0);
}

// bugs/linalg/eig-left-vectors.md — 3rd output W (left eigenvectors).
TEST_F(LinalgKnownBug, DISABLED_EigLeftVectors)
{
    eval("A = [4 -2; 1 1]; [V, D, W] = eig(A);");
    // Left eigenvectors satisfy W'*A = D*W'.
    EXPECT_LT(evalScalar("max(max(abs(W'*A - D*W')))"), 1e-10);
}

// bugs/linalg/norm-complex.md — norm of a complex array (vecnorm handles it).
TEST_F(LinalgKnownBug, DISABLED_NormComplex)
{
    EXPECT_NEAR(evalScalar("norm([3+4i 0])"), 5.0, 1e-12);        // 2-norm
    EXPECT_NEAR(evalScalar("norm([3+4i 0], 'fro')"), 5.0, 1e-12); // Frobenius
}
