// libs/builtin/tests/matfunc_test.cpp
//
// Regression guard for matrix functions: expm / logm / sqrtm / schur.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>
#include <cmath>

using namespace numkit;

class MatFuncTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── expm ────────────────────────────────────────────────────

TEST_F(MatFuncTest, ExpmOfZeroIsIdentity)
{
    eval("E = expm(zeros(3));");
    EXPECT_DOUBLE_EQ(evalScalar("max(max(abs(E - eye(3))))"), 0.0);
}

TEST_F(MatFuncTest, ExpmOfRotationGenerator)
{
    // expm([0 1; -1 0]) = rotation by 1 rad: [cos, sin; -sin, cos].
    eval("E = expm([0 1; -1 0]);");
    EXPECT_NEAR(evalScalar("E(1,1)"),  std::cos(1.0), 1e-12);
    EXPECT_NEAR(evalScalar("E(1,2)"),  std::sin(1.0), 1e-12);
    EXPECT_NEAR(evalScalar("E(2,1)"), -std::sin(1.0), 1e-12);
    EXPECT_NEAR(evalScalar("E(2,2)"),  std::cos(1.0), 1e-12);
}

TEST_F(MatFuncTest, ExpmDiagonal)
{
    // expm(diag(d)) = diag(exp(d)).
    eval("E = expm(diag([1 2 3]));");
    EXPECT_NEAR(evalScalar("E(1,1)"), std::exp(1.0), 1e-12);
    EXPECT_NEAR(evalScalar("E(2,2)"), std::exp(2.0), 1e-12);
    EXPECT_NEAR(evalScalar("E(3,3)"), std::exp(3.0), 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("E(1,2)"), 0.0);
}

// ── logm (symmetric SPD) ────────────────────────────────────

TEST_F(MatFuncTest, LogmRoundTrip)
{
    // expm(logm(A)) == A for symmetric SPD.
    eval("S = [4 1 2; 1 3 0; 2 0 5]; L = logm(S);");
    EXPECT_NEAR(evalScalar("max(max(abs(expm(L) - S)))"), 0.0, 1e-10);
}

TEST_F(MatFuncTest, LogmIdentityIsZero)
{
    eval("L = logm(eye(4));");
    EXPECT_NEAR(evalScalar("max(max(abs(L)))"), 0.0, 1e-12);
}

TEST_F(MatFuncTest, LogmAsymmetricRejected)
{
    EXPECT_THROW(eval("logm([1 2; 3 4]);"), std::exception);
}

TEST_F(MatFuncTest, LogmNegativeEigenvalueRejected)
{
    // -I has eigenvalue -1 -> log(-1) = NaN, should throw.
    EXPECT_THROW(eval("logm(-eye(3));"), std::exception);
}

// ── sqrtm (symmetric PSD) ───────────────────────────────────

TEST_F(MatFuncTest, SqrtmRoundTrip)
{
    eval("S = [4 1 2; 1 3 0; 2 0 5]; R = sqrtm(S);");
    EXPECT_NEAR(evalScalar("max(max(abs(R*R - S)))"), 0.0, 1e-12);
}

TEST_F(MatFuncTest, SqrtmIdentityIsIdentity)
{
    eval("R = sqrtm(eye(5));");
    EXPECT_NEAR(evalScalar("max(max(abs(R - eye(5))))"), 0.0, 1e-12);
}

// ── schur (symmetric -> diagonal T) ─────────────────────────

TEST_F(MatFuncTest, SchurOfSymmetricGivesDiagonalT)
{
    eval("S = [4 1 2; 1 3 0; 2 0 5]; [U, T] = schur(S);");
    // U*T*U' == S
    EXPECT_NEAR(evalScalar("max(max(abs(U*T*U' - S)))"), 0.0, 1e-12);
    // T strictly diagonal.
    EXPECT_DOUBLE_EQ(evalScalar("T(1,2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(2,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(1,3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(3,1)"), 0.0);
}

TEST_F(MatFuncTest, SchurAsymmetricRejected)
{
    // General Schur is Phase 2b -- asymmetric must throw for now.
    EXPECT_THROW(eval("schur([1 2; 3 4]);"), std::exception);
}
