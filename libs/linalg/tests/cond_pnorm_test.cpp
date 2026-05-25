// libs/linalg/tests/cond_pnorm_test.cpp
//
// Regression guard for the cond(A, p) extension (p ∈ {1, 2, Inf, 'fro'}).
// Closes the ⚠️ gap in PROGRESS — cond was 2-norm only.
// Spec at tools/parity/specs/cond_pnorm.json (OK vs MATLAB R2025b).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class CondPnormTest : public ::testing::Test
{
public:
    Engine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Diagonal: cond(A, p) for p ∈ {1, 2, Inf} == max|d|/min|d|.
TEST_F(CondPnormTest, DiagonalAllNormsMatch)
{
    eval("A = diag([1 1e-3]); "
         "c1 = cond(A, 1); c2 = cond(A, 2); cInf = cond(A, Inf);");
    EXPECT_NEAR(evalScalar("c1"),   1e3, 1e-9);
    EXPECT_NEAR(evalScalar("c2"),   1e3, 1e-9);
    EXPECT_NEAR(evalScalar("cInf"), 1e3, 1e-9);
}

// Frobenius: cond(A, 'fro') == norm(A,'fro') · norm(inv(A),'fro').
TEST_F(CondPnormTest, FrobeniusMatchesNormProduct)
{
    eval("A = diag([1 1e-3]); cF = cond(A, 'fro');"
         "ref = norm(A, 'fro') * norm(inv(A), 'fro');"
         "err = abs(cF - ref);");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

// 'inf' string form == numeric Inf.
TEST_F(CondPnormTest, InfStringEqualsInfNumeric)
{
    eval("A = [1 2; 3 4];"
         "c_str = cond(A, 'inf');"
         "c_num = cond(A, Inf);");
    EXPECT_DOUBLE_EQ(evalScalar("c_str"), evalScalar("c_num"));
}

// 1-output (no p) defaults to p == 2.
TEST_F(CondPnormTest, DefaultIsTwoNorm)
{
    eval("A = [1 2; 3 4];"
         "c_def = cond(A);"
         "c_2   = cond(A, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("c_def"), evalScalar("c_2"));
}

// Identity has condition number 1 in every norm.
TEST_F(CondPnormTest, IdentityIsOneEverywhere)
{
    EXPECT_DOUBLE_EQ(evalScalar("cond(eye(3), 1)"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("cond(eye(3), 2)"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("cond(eye(3), Inf)"),   1.0);
    EXPECT_NEAR(evalScalar("cond(eye(3), 'fro')"), 3.0, 1e-12);  // ||I||_F = sqrt(n)
}

// Singular A → Inf condition.
TEST_F(CondPnormTest, SingularGivesInfinity)
{
    EXPECT_TRUE(std::isinf(evalScalar("cond([1 1; 1 1], 1)")));
    EXPECT_TRUE(std::isinf(evalScalar("cond([1 1; 1 1], Inf)")));
}
