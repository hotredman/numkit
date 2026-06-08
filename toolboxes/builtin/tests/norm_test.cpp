// toolboxes/builtin/tests/norm_test.cpp
//
// Regression guard for builtin::norm and asymmetric eig [V,D] form.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class NormTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── vector norms ───────────────────────────────────────────

TEST_F(NormTest, NormVectorEuclidean)
{
    EXPECT_DOUBLE_EQ(evalScalar("norm([3 4])"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("norm([3; 4])"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("norm([1 2 2])"), 3.0);
}

TEST_F(NormTest, NormVector1)
{
    EXPECT_DOUBLE_EQ(evalScalar("norm([3 4], 1)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("norm([-2 5 -1], 1)"), 8.0);
}

TEST_F(NormTest, NormVectorInf)
{
    EXPECT_DOUBLE_EQ(evalScalar("norm([3 4], Inf)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("norm([-7 2 5], Inf)"), 7.0);
}

TEST_F(NormTest, NormVectorNegInf)
{
    // p = -Inf vector norm is min(|v|). Both +Inf and -Inf used to route to
    // max(|v|); -Inf must give the minimum.
    EXPECT_DOUBLE_EQ(evalScalar("norm([3 4], -Inf)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("norm([3; 4], -Inf)"), 3.0);   // column
    EXPECT_DOUBLE_EQ(evalScalar("norm([-7 2 5], -Inf)"), 2.0); // min |.|
    EXPECT_DOUBLE_EQ(evalScalar("norm(5, -Inf)"), 5.0);        // scalar
}

TEST_F(NormTest, NormMatrixNegInfThrows)
{
    // MATLAB: -Inf is not a valid matrix norm.
    EXPECT_THROW(eval("norm([1 2; 3 4], -Inf);"), std::exception);
}

TEST_F(NormTest, NormVectorPnorm)
{
    // p=4: (3^4 + 4^4)^(1/4) = (81+256)^0.25 = 337^0.25 ≈ 4.286
    EXPECT_NEAR(evalScalar("norm([3 4], 4)"), 4.2854, 1e-3);
}

TEST_F(NormTest, NormEmptyIsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("norm([])"), 0.0);
}

// ── matrix norms ───────────────────────────────────────────

TEST_F(NormTest, NormMatrix2IsLargestSigma)
{
    // norm(A) = svd(A)(1).
    eval("A = [1 2; 3 4]; n2 = norm(A); s = svd(A);");
    EXPECT_NEAR(evalScalar("n2 - s(1)"), 0.0, 1e-12);
}

TEST_F(NormTest, NormMatrix1MaxColSum)
{
    EXPECT_DOUBLE_EQ(evalScalar("norm([1 2; 3 4], 1)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("norm(eye(5), 1)"), 1.0);
}

TEST_F(NormTest, NormMatrixInfMaxRowSum)
{
    EXPECT_DOUBLE_EQ(evalScalar("norm([1 2; 3 4], Inf)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("norm(eye(5), Inf)"), 1.0);
}

TEST_F(NormTest, NormMatrixFrobenius)
{
    // sqrt(1 + 4 + 9 + 16) = sqrt(30).
    EXPECT_NEAR(evalScalar("norm([1 2; 3 4], 'fro')"), std::sqrt(30.0), 1e-12);
}

// ── asymmetric eig [V, D] for real eigenvalues ─────────────

TEST_F(NormTest, AsymmetricEigVDForRealEigvals)
{
    // Companion of x^3 - 7x + 6 (eigenvalues 1, 2, -3).
    eval("A = [0 7 -6; 1 0 0; 0 1 0]; [V, D] = eig(A);");
    // A*V == V*D verified.
    EXPECT_NEAR(evalScalar("max(max(abs(A*V - V*D)))"), 0.0, 1e-10);
    // Diagonal of D contains the three eigenvalues (sorted ascending).
    eval("d = sort(diag(D));");
    EXPECT_NEAR(evalScalar("d(1)"), -3.0, 1e-10);
    EXPECT_NEAR(evalScalar("d(2)"),  1.0, 1e-10);
    EXPECT_NEAR(evalScalar("d(3)"),  2.0, 1e-10);
}

TEST_F(NormTest, AsymmetricEigComplexThrowsClean)
{
    // [V, D] for matrix with complex eigvals → throws.
    EXPECT_THROW(eval("[V, D] = eig([0 -1; 1 0]);"), std::exception);
}
