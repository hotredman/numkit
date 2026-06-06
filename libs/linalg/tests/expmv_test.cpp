// libs/linalg/tests/expmv_test.cpp
//
// Regression guard for expmv = exp(t·A)·v via Krylov subspace.
// MATLAB core does NOT ship expmv (it's in Higham's File Exchange
// package), so the spec is correctness=N/A and we rely on algebraic
// identity vs the full expm() path for cross-validation.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class ExpmvTest : public ::testing::Test
{
public:
    StdEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Diagonal A: exp(t·A)·v is elementwise exact.
TEST_F(ExpmvTest, DiagonalGivesElementwiseExp)
{
    eval("A = diag([1 2 3]); v = [1; 1; 1];"
         "w = expmv(0.5, A, v);");
    EXPECT_NEAR(evalScalar("w(1)"), std::exp(0.5), 1e-13);
    EXPECT_NEAR(evalScalar("w(2)"), std::exp(1.0), 1e-13);
    EXPECT_NEAR(evalScalar("w(3)"), std::exp(1.5), 1e-13);
}

// Triangular A: matches expm(t*A) * v to ulp.
TEST_F(ExpmvTest, MatchesFullExpmOnTriangular)
{
    eval("A = [0.1 0.2 0; 0 0.3 0.4; 0 0 0.5];"
         "v = [1; 2; 3];"
         "w1 = expmv(0.7, A, v);"
         "w2 = expm(0.7 * A) * v;"
         "err = max(abs(w1 - w2));");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

// Linearity: expmv(t, A, v1+v2) == expmv(t, A, v1) + expmv(t, A, v2).
TEST_F(ExpmvTest, Linearity)
{
    eval("A = [1 0.5; -0.5 1];"
         "v1 = [1; 0]; v2 = [0; 1];"
         "lhs = expmv(0.3, A, v1 + v2);"
         "rhs = expmv(0.3, A, v1) + expmv(0.3, A, v2);"
         "err = max(abs(lhs - rhs));");
    EXPECT_LT(evalScalar("err"), 1e-13);
}

// Zero vector → zero output.
TEST_F(ExpmvTest, ZeroVectorReturnsZero)
{
    eval("A = magic(4); w = expmv(1.0, A, zeros(4, 1));");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(w))"), 0.0);
}

// t = 0: w == v identically (exp(0) = I).
TEST_F(ExpmvTest, TimeZeroIsIdentity)
{
    eval("A = [2 1; -1 3]; v = [4; 7];"
         "w = expmv(0.0, A, v);"
         "err = max(abs(w - v));");
    EXPECT_LT(evalScalar("err"), 1e-13);
}
