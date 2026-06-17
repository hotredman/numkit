// toolboxes/builtin/tests/lsq_test.cpp
//
// Regression guard for linalg cycle 4:
//   lsqminnorm(A, B [, tol])  pinv-based minimum-norm LS
//   lsqnonneg(C, d)           Lawson-Hanson active-set NNLS

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class LsqTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── lsqminnorm ────────────────────────────────────────────────────────
TEST_F(LsqTest, MinnormFullRankSquareMatchesBackslash)
{
    eval("x = lsqminnorm([1 2; 3 4], [5; 11]);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 2.0, 1e-12);
}

TEST_F(LsqTest, MinnormRankDeficientReturnsMinNormSolution)
{
    // [1 2; 2 4; 3 6] is rank-1 (col2 = 2*col1). For b = [3; 6; 9],
    // the min-norm LS solution is x = [3/5; 6/5].
    eval("x = lsqminnorm([1 2; 2 4; 3 6], [3; 6; 9]);"
         "res = norm([1 2; 2 4; 3 6]*x - [3; 6; 9]);"
         "nx = norm(x);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.6, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 1.2, 1e-12);
    EXPECT_LT(evalScalar("res"), 1e-12);
    EXPECT_NEAR(evalScalar("nx"), std::sqrt(0.36 + 1.44), 1e-12);
}

TEST_F(LsqTest, MinnormWideUnderdetermined)
{
    // 2×3 system: x = [1; 1; 1] is the min-norm solution for b = [6; 15].
    eval("x = lsqminnorm([1 2 3; 4 5 6], [6; 15]);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 1.0, 1e-12);
}

TEST_F(LsqTest, MinnormBMatrixMultipleRhs)
{
    eval("X = lsqminnorm([1 0; 0 0], [1 2; 3 4]);");
    EXPECT_NEAR(evalScalar("X(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("X(1,2)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("X(2,1)"), 0.0, 1e-12);  // null direction zeroed
    EXPECT_NEAR(evalScalar("X(2,2)"), 0.0, 1e-12);
}

TEST_F(LsqTest, MinnormDimMismatchThrows)
{
    bool threw = false;
    try { eval("lsqminnorm([1 2; 3 4], [1; 2; 3]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// ── lsqnonneg ─────────────────────────────────────────────────────────
TEST_F(LsqTest, NnlsLawsonHansonClassic)
{
    // Classic textbook input.
    eval("[x, rn, res, ef] = lsqnonneg([1 -1 2; 3 4 5; 6 7 8], [1; 2; 3]);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 0.387096774193548, 1e-9);
    EXPECT_NEAR(evalScalar("rn"), 0.06451612903, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("ef"), 1.0);
}

TEST_F(LsqTest, NnlsAllNonnegSolutionMatchesLS)
{
    // No active constraints: lsqnonneg should return the unconstrained LS
    // solution, which is just [1; 2] for diagonal C.
    eval("x = lsqnonneg([1 0; 0 1], [1; 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(2)"), 2.0);
}

TEST_F(LsqTest, NnlsClampsNegativeTarget)
{
    // d = [3; -2] would give x = [3; -2] unconstrained, but constraint
    // forces x(2) = 0.
    eval("x = lsqnonneg([1 0; 0 1], [3; -2]);");
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(2)"), 0.0);
}

TEST_F(LsqTest, NnlsZeroRhsZeroSolution)
{
    eval("[x, rn] = lsqnonneg([1 2; 3 4], [0; 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("rn"), 0.0);
}

TEST_F(LsqTest, NnlsResidualMatchesDMinusCx)
{
    // residual = d - C*x by construction.
    eval("[x, rn, res] = lsqnonneg([1 -1 2; 3 4 5; 6 7 8], [1; 2; 3]);"
         "exp_res = [1; 2; 3] - [1 -1 2; 3 4 5; 6 7 8] * x;"
         "diff = max(abs(res - exp_res));");
    EXPECT_LT(evalScalar("diff"), 1e-12);
}

TEST_F(LsqTest, NnlsOutputStruct)
{
    eval("[x, rn, res, ef, out] = lsqnonneg([1 -1 2; 3 4 5; 6 7 8], [1; 2; 3]);"
         "iter = out.iterations;"
         "alg = out.algorithm;");
    EXPECT_GE(evalScalar("iter"), 1.0);
    // Algorithm name should be 'active-set' (char array).
    Value alg = eval("out.algorithm");
    EXPECT_TRUE(alg.isChar());
}

TEST_F(LsqTest, NnlsRejectsBadDims)
{
    bool threw = false;
    try { eval("lsqnonneg([1 2; 3 4], [1; 2; 3]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(LsqTest, NnlsExitflagPositive)
{
    eval("[~, ~, ~, ef] = lsqnonneg([1 0; 0 1], [3; -2]);");
    EXPECT_DOUBLE_EQ(evalScalar("ef"), 1.0);
}
