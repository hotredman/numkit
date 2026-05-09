// libs/builtin/tests/eig_test.cpp
//
// Regression guard for symmetric eig (Phase 2 -- Jacobi rotations).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EigTest : public ::testing::Test
{
public:
    Engine engine;
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

TEST_F(EigTest, EigAsymmetricRejected)
{
    EXPECT_THROW(eval("eig([1 2; 3 4]);"), std::exception);
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
