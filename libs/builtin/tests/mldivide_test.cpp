// libs/builtin/tests/mldivide_test.cpp
// builtin/mrdivide (and the symmetric mldivide).
// Covers the matrix-solve paths:
//   - Square A : LU with partial pivoting
//   - Tall A   : QR via Householder + R back-solve (least squares)
//   - Wide A   : explicit error (deferred)
//   - Scalar/scalar and matrix/scalar : pre-existing elementwise paths
// All hardcoded expected values verified bit-identical vs MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MldivideTest : public ::testing::Test
{
public:
    StdEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ─── mldivide: square 2×2 LU path ────────────────────────────────────

TEST_F(MldivideTest, SquareSolve2x2)
{
    eval("A = [1 2; 3 4]; B = [5 6; 7 8];");
    eval("X = A \\ B;");
    EXPECT_NEAR(evalScalar("X(1,1)"), -3.0, 1e-12);
    EXPECT_NEAR(evalScalar("X(2,1)"),  4.0, 1e-12);
    EXPECT_NEAR(evalScalar("X(1,2)"), -4.0, 1e-12);
    EXPECT_NEAR(evalScalar("X(2,2)"),  5.0, 1e-12);
}

TEST_F(MldivideTest, SquareSolveSatisfiesEquation)
{
    eval("A = [1 2; 3 4]; B = [5 6; 7 8];");
    eval("X = A \\ B;");
    eval("R = A * X - B;");
    EXPECT_LT(evalScalar("max(abs(R(:)))"), 1e-12);
}

// ─── mldivide: square SPD-like 3×3 (verifies pivot logic) ────────────

TEST_F(MldivideTest, SquareSolve3x3)
{
    eval("A = [4 1 2; 1 5 3; 2 3 6];");
    eval("b = [1; 2; 3];");
    eval("x = A \\ b;");
    // MATLAB: [0; 0.142857142857143; 0.428571428571429]
    EXPECT_NEAR(evalScalar("x(1)"), 0.0,                 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 0.142857142857143,   1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 0.428571428571429,   1e-12);
}

TEST_F(MldivideTest, SquareMultiRhs)
{
    eval("A = [4 -2 1; -2 5 3; 1 3 6];");
    eval("B = [1 2; 3 4; 5 6];");
    eval("X = A \\ B;");
    EXPECT_NEAR(evalScalar("X(1,1)"), 0.255813953488372, 1e-12);
    EXPECT_NEAR(evalScalar("X(2,1)"), 0.325581395348837, 1e-12);
    EXPECT_NEAR(evalScalar("X(3,1)"), 0.627906976744186, 1e-12);
    EXPECT_NEAR(evalScalar("X(1,2)"), 0.837209302325581, 1e-12);
    EXPECT_NEAR(evalScalar("X(2,2)"), 0.883720930232558, 1e-12);
    EXPECT_NEAR(evalScalar("X(3,2)"), 0.418604651162791, 1e-12);
}

TEST_F(MldivideTest, RequiresPartialPivoting)
{
    // Pivot reordering required: the (1,1) entry is small relative to
    // the (2,1) entry, so naive Gaussian elimination loses precision.
    eval("A = [1e-15 1; 1 1];");
    eval("b = [1; 2];");
    eval("x = A \\ b;");
    eval("R = A * x - b;");
    EXPECT_LT(evalScalar("max(abs(R))"), 1e-12);
}

// ─── mldivide: tall A → least squares via QR ─────────────────────────

TEST_F(MldivideTest, TallLeastSquaresLine)
{
    // Best-fit line y = a + b·x for points (0,6) (1,5) (2,7) (3,10).
    // MATLAB: [4.9; 1.4]
    eval("A = [1 0; 1 1; 1 2; 1 3];");
    eval("b = [6; 5; 7; 10];");
    eval("x = A \\ b;");
    EXPECT_NEAR(evalScalar("x(1)"), 4.9, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 1.4, 1e-12);
}

TEST_F(MldivideTest, TallExactSolutionWhenConsistent)
{
    // Tall but rank-2 system with a consistent RHS — least-squares
    // should recover the exact answer (zero residual).
    eval("A = [1 0; 0 1; 1 1];");
    eval("x_true = [2; 3];");
    eval("b = A * x_true;");
    eval("x = A \\ b;");
    EXPECT_NEAR(evalScalar("x(1)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 3.0, 1e-12);
}

// ─── mldivide: scalar paths (pre-existing) ───────────────────────────

TEST_F(MldivideTest, ScalarDivision)
{
    EXPECT_DOUBLE_EQ(evalScalar("6 \\ 2"), 1.0 / 3.0);
}

TEST_F(MldivideTest, ScalarMatrixIsElementwise)
{
    eval("X = 2 \\ [4 6 8];");
    EXPECT_DOUBLE_EQ(evalScalar("X(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("X(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("X(3)"), 4.0);
}

// ─── mldivide: error paths ───────────────────────────────────────────

TEST_F(MldivideTest, SingularMatrixThrows)
{
    eval("A = [1 2; 2 4];");  // rank 1
    eval("b = [3; 6];");
    bool threw = false;
    try { eval("x = A \\ b;"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(MldivideTest, WideMatrixThrows)
{
    eval("A = [1 2 3; 4 5 6];");
    eval("b = [1; 2];");
    bool threw = false;
    try { eval("x = A \\ b;"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(MldivideTest, DimensionMismatchThrows)
{
    eval("A = [1 2; 3 4];");
    eval("b = [1; 2; 3];");
    bool threw = false;
    try { eval("x = A \\ b;"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}

// ════════════════════════════════════════════════════════════════════
// mrdivide  —  X = A / B  ↔  X · B = A  ↔  X = (B'\A')'
// ════════════════════════════════════════════════════════════════════

class MrdivideTest : public ::testing::Test
{
public:
    StdEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(MrdivideTest, SquareSolve2x2)
{
    eval("A = [1 2; 3 4]; B = [5 6; 7 8];");
    eval("X = A / B;");
    // MATLAB: [3 -2; 2 -1]
    EXPECT_NEAR(evalScalar("X(1,1)"),  3.0, 1e-12);
    EXPECT_NEAR(evalScalar("X(1,2)"), -2.0, 1e-12);
    EXPECT_NEAR(evalScalar("X(2,1)"),  2.0, 1e-12);
    EXPECT_NEAR(evalScalar("X(2,2)"), -1.0, 1e-12);
}

TEST_F(MrdivideTest, SatisfiesEquation)
{
    eval("A = [1 2; 3 4]; B = [5 6; 7 8];");
    eval("X = A / B;");
    eval("R = X * B - A;");
    EXPECT_LT(evalScalar("max(abs(R(:)))"), 1e-12);
}

TEST_F(MrdivideTest, RowVectorDivision)
{
    // A is 1×3, B is 3×3 — X = (B'\A')' is a 1×3 row vector.
    eval("A = [1 2 3]; B = [4 1 2; 1 5 3; 2 3 6];");
    eval("X = A / B;");
    EXPECT_NEAR(evalScalar("X(1)"), 0.0,               1e-12);
    EXPECT_NEAR(evalScalar("X(2)"), 0.142857142857143, 1e-12);
    EXPECT_NEAR(evalScalar("X(3)"), 0.428571428571429, 1e-12);
}

TEST_F(MrdivideTest, MatrixOverScalarIsElementwise)
{
    eval("X = [4 6 8] / 2;");
    EXPECT_DOUBLE_EQ(evalScalar("X(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("X(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("X(3)"), 4.0);
}

TEST_F(MrdivideTest, ScalarOverMatrixThrows)
{
    // Per MATLAB R2025b: `2 / [1 2; 3 4]` errors with
    // "Matrix dimensions must agree" — do NOT silently expand.
    bool threw = false;
    try { eval("X = 2 / [1 2; 3 4];"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(MrdivideTest, ScalarScalarStillWorks)
{
    EXPECT_DOUBLE_EQ(evalScalar("6 / 2"), 3.0);
}
