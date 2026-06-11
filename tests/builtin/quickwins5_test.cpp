// toolboxes/builtin/tests/quickwins5_test.cpp
//
// Regression guard for the cycle-51 quick-wins five:
//   trace, det, chol, topkrows, cputime

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class QuickWins5Test : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── trace ──────────────────────────────────────────────────

TEST_F(QuickWins5Test, TraceSquare)
{
    EXPECT_DOUBLE_EQ(evalScalar("trace([1 2 3; 4 5 6; 7 8 9])"), 15.0);
    EXPECT_DOUBLE_EQ(evalScalar("trace(eye(7))"),                  7.0);
    EXPECT_DOUBLE_EQ(evalScalar("trace(zeros(4))"),                0.0);
}

TEST_F(QuickWins5Test, TraceRectangular)
{
    // min(rows,cols) elements summed.
    EXPECT_DOUBLE_EQ(evalScalar("trace([1 2; 3 4; 5 6])"), 5.0);  // 1 + 4
    EXPECT_DOUBLE_EQ(evalScalar("trace([1 2 3; 4 5 6])"), 6.0);   // 1 + 5
}

// ── det ────────────────────────────────────────────────────

TEST_F(QuickWins5Test, Det2x2)
{
    EXPECT_DOUBLE_EQ(evalScalar("det([4 7; 2 6])"),  10.0);
    EXPECT_DOUBLE_EQ(evalScalar("det([1 2; 3 4])"),  -2.0);
}

TEST_F(QuickWins5Test, DetTriangular)
{
    // Upper-triangular det = product of diagonal.
    EXPECT_DOUBLE_EQ(evalScalar("det([1 2 3; 0 5 6; 0 0 9])"), 45.0);
}

TEST_F(QuickWins5Test, DetIdentity)
{
    EXPECT_DOUBLE_EQ(evalScalar("det(eye(5))"), 1.0);
}

TEST_F(QuickWins5Test, DetSingularReturnsZero)
{
    // Rank-1 matrix: det == 0.
    EXPECT_NEAR(evalScalar("det([1 2; 2 4])"), 0.0, 1e-12);
}

TEST_F(QuickWins5Test, DetNonSquareRejected)
{
    EXPECT_THROW(eval("det([1 2 3; 4 5 6]);"), std::exception);
}

// ── chol ───────────────────────────────────────────────────

TEST_F(QuickWins5Test, CholFactorRoundtrip)
{
    eval("S = [4 12 -16; 12 37 -43; -16 -43 98]; R = chol(S);");
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"),  2.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,2)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(3,3)"),  3.0);
    // Lower triangle of R should be all zeros.
    EXPECT_DOUBLE_EQ(evalScalar("R(2,1)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(3,1)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(3,2)"),  0.0);
    // R'*R == S exactly (small SPD example, no rounding).
    EXPECT_DOUBLE_EQ(evalScalar("max(max(abs(R'*R - S)))"), 0.0);
}

TEST_F(QuickWins5Test, CholNotPositiveDefiniteRejected)
{
    EXPECT_THROW(eval("chol([1 2; 2 1]);"), std::exception);  // indefinite
    EXPECT_THROW(eval("chol([0 0; 0 0]);"), std::exception);  // zero pivot
    EXPECT_THROW(eval("chol([-1 0; 0 -1]);"), std::exception); // negative
}

TEST_F(QuickWins5Test, CholNonSquareRejected)
{
    EXPECT_THROW(eval("chol([1 2 3; 4 5 6]);"), std::exception);
}

// chol(A,'lower') returns lower-triangular L with L*L' = A (= R'). vs MATLAB.
TEST_F(QuickWins5Test, CholLowerOption)
{
    eval("A = [4 2 2; 2 5 1; 2 1 6]; L = chol(A,'lower');");
    EXPECT_DOUBLE_EQ(evalScalar("L(1,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(2,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(3,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(2,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(1,2)"), 0.0);   // strictly lower triangular
    EXPECT_DOUBLE_EQ(evalScalar("L(1,3)"), 0.0);
    EXPECT_NEAR(evalScalar("max(max(abs(L*L' - A)))"), 0.0, 1e-12);
    // case-insensitive option + explicit 'upper' == default.
    EXPECT_DOUBLE_EQ(evalScalar("R=chol(A,'UPPER'); R(1,1)"), 2.0);
}

// [R,p] = chol(A): p=0 when PD; p=failure column (no error) when not PD,
// with R = the leading (p-1)x(p-1) factor. vs MATLAB R2025b.
TEST_F(QuickWins5Test, CholPivotSecondOutput)
{
    eval("[R,p] = chol([4 2; 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"), 2.0);
    // Indefinite 2x2: p=2, R is the 1x1 factor of the leading [1] block.
    eval("[R2,p2] = chol([1 3; 3 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("p2"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(R2,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(R2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R2(1,1)"), 1.0);
    // [L,p] honors 'lower' on success.
    eval("[L,pl] = chol([4 2; 2 3],'lower');");
    EXPECT_DOUBLE_EQ(evalScalar("pl"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(2,1)"), 1.0);
    // 1-output non-PD still throws.
    EXPECT_THROW(eval("chol([1 3; 3 1]);"), std::exception);
}

// ── topkrows ───────────────────────────────────────────────

TEST_F(QuickWins5Test, TopKRowsLexDescending)
{
    eval("M = [3 1; 1 5; 2 4; 5 2; 4 3]; T = topkrows(M, 3);");
    // Lex-descending by all columns: (5,2), (4,3), (3,1), (2,4), (1,5).
    EXPECT_DOUBLE_EQ(evalScalar("T(1,1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(1,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(2,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(2,2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(3,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(3,2)"), 1.0);
}

TEST_F(QuickWins5Test, TopKRowsAllRows)
{
    eval("M = [3; 1; 2]; T = topkrows(M, 5);");  // k > rows -> all rows
    EXPECT_EQ(static_cast<int>(evalScalar("size(T,1)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("T(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(3)"), 1.0);
}

TEST_F(QuickWins5Test, TopKRowsZero)
{
    eval("T = topkrows([1; 2; 3], 0);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(T)")), 0);
}

// ── cputime ────────────────────────────────────────────────

TEST_F(QuickWins5Test, CputimePositive)
{
    EXPECT_GE(evalScalar("cputime"), 0.0);
}

TEST_F(QuickWins5Test, CputimeMonotonicNonDecreasing)
{
    eval("a = cputime;");
    // Burn a few microseconds.
    eval("for k = 1:10000; x = sin(k); end;");
    eval("b = cputime;");
    EXPECT_GE(evalScalar("b"), evalScalar("a"));
}
