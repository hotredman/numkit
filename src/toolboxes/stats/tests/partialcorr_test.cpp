// toolboxes/stats/tests/partialcorr_test.cpp
//
// Regression guard for partialcorr.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class PartialCorrTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(PartialCorrTest, PartialcorrDeterministic)
{
    eval("x = [1; 2; 3; 4; 5; 6; 7; 8; 9; 10]; "
         "y = [1.2; 1.8; 3.5; 3.9; 5.1; 6.0; 7.2; 7.8; 9.1; 10.0]; "
         "z = [1; 4; 2; 5; 3; 6; 8; 7; 9; 10]; "
         "p = partialcorr(x, y, z);");
    EXPECT_NEAR(evalScalar("p"), 0.987889, 1e-5);
}

TEST_F(PartialCorrTest, PartialcorrShape)
{
    // X is m×2, Y is m×3 → result is 2×3.
    eval("X = [1 2; 3 4; 5 6; 7 8; 9 10]; "
         "Y = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11]; "
         "Z = [1; 1; 2; 2; 3]; "
         "P = partialcorr(X, Y, Z);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,2)")), 3);
}

TEST_F(PartialCorrTest, PartialcorrDimMismatchThrows)
{
    EXPECT_THROW(eval("partialcorr([1;2;3], [1;2], [1;2;3]);"), std::exception);
}

TEST_F(PartialCorrTest, PartialcorrControlsForConfounder)
{
    // Construct: x and y both depend strongly on z.
    // Conditional on z, residuals should have low correlation.
    eval("z = [1; 2; 3; 4; 5; 6; 7; 8; 9; 10]; "
         "x = z + [0.1; -0.2; 0.05; 0.15; -0.1; 0.2; -0.05; 0.1; -0.15; 0.05]; "
         "y = z + [-0.1; 0.2; 0.15; -0.1; 0.05; -0.2; 0.1; 0.05; 0.15; -0.1]; "
         "p_naive = corr([x y]); p_part = partialcorr(x, y, z);");
    // naive should be high (>0.99 since trend dominates)
    EXPECT_GT(evalScalar("p_naive(1, 2)"), 0.99);
    // partial should be smaller than naive (residuals after regressing
    // out z still have some structure since hand-picked noise isn't
    // truly random, but the trend is removed).
    EXPECT_LT(std::fabs(evalScalar("p_part")),
              std::fabs(evalScalar("p_naive(1, 2)")));
}

// ── partialcorr(X) — 1-arg form (cycle 85) ──────────────────────────

TEST_F(PartialCorrTest, OneArgMatrix)
{
    // MATLAB reference: partialcorr([1 2 3; 4 1 5; ...]) computed once.
    eval("X = [1 2 3; 4 1 5; 7 5 2; 2 8 6; 9 3 7; 5 6 4; 3 9 8; 8 4 1]; "
         "R = partialcorr(X);");
    // Shape + diagonal.
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,2)")), 3);
    EXPECT_NEAR(evalScalar("R(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("R(2,2)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("R(3,3)"), 1.0, 1e-12);
    // MATLAB R2025b values:
    EXPECT_NEAR(evalScalar("R(1,2)"), -0.137849207998928, 1e-10);
    EXPECT_NEAR(evalScalar("R(1,3)"), -0.163626829119191, 1e-10);
    EXPECT_NEAR(evalScalar("R(2,3)"),  0.361968077223058, 1e-10);
    // Symmetry.
    EXPECT_NEAR(evalScalar("R(2,1)"), evalScalar("R(1,2)"), 1e-14);
    EXPECT_NEAR(evalScalar("R(3,1)"), evalScalar("R(1,3)"), 1e-14);
    EXPECT_NEAR(evalScalar("R(3,2)"), evalScalar("R(2,3)"), 1e-14);
}

TEST_F(PartialCorrTest, OneArgTwoColsEqualsCorr)
{
    // When X has exactly 2 columns, control set is empty (only the
    // intercept) → partialcorr(X) equals corr(X).
    eval("X2 = [1 2; 3 5; 5 4; 7 8; 9 7]; "
         "Rp = partialcorr(X2); Rc = corr(X2);");
    EXPECT_NEAR(evalScalar("Rp(1,2)"), evalScalar("Rc(1,2)"), 1e-12);
    EXPECT_NEAR(evalScalar("Rp(1,2)"), 0.860946032092278, 1e-10);
}

TEST_F(PartialCorrTest, OneArgSingleColumn)
{
    // Single-column X → 1×1 matrix [1].
    eval("X1 = [1; 2; 3; 4; 5]; R = partialcorr(X1);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,2)")), 1);
    EXPECT_NEAR(evalScalar("R(1,1)"), 1.0, 1e-14);
}

// ── partialcorr(X, Z) — 2-arg form (cycle 85) ───────────────────────

TEST_F(PartialCorrTest, TwoArgMatrixZ)
{
    eval("X = [1 2 3; 4 1 5; 7 5 2; 2 8 6; 9 3 7; 5 6 4; 3 9 8; 8 4 1]; "
         "Z = [1 -1; 2 -2; 3 -1; 1 -3; 2 -2; 4 -1; 3 -3; 1 -1]; "
         "R = partialcorr(X, Z);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,2)")), 3);
    EXPECT_NEAR(evalScalar("R(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("R(2,2)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("R(3,3)"), 1.0, 1e-12);
    // MATLAB R2025b values:
    EXPECT_NEAR(evalScalar("R(1,2)"), -0.117717461591268, 1e-10);
    EXPECT_NEAR(evalScalar("R(1,3)"),  0.064511203195569, 1e-10);
    EXPECT_NEAR(evalScalar("R(2,3)"), -0.544059070513052, 1e-10);
    // Symmetry.
    EXPECT_NEAR(evalScalar("R(2,1)"), evalScalar("R(1,2)"), 1e-14);
    EXPECT_NEAR(evalScalar("R(3,2)"), evalScalar("R(2,3)"), 1e-14);
}

TEST_F(PartialCorrTest, TwoArgEqualsXXZ)
{
    // partialcorr(X, Z) must equal partialcorr(X, X, Z) (with diag
    // adjusted to exactly 1). Verify a few off-diagonals.
    eval("X = [1 2 3; 4 1 5; 7 5 2; 2 8 6; 9 3 7; 5 6 4; 3 9 8; 8 4 1]; "
         "Z = [1 -1; 2 -2; 3 -1; 1 -3; 2 -2; 4 -1; 3 -3; 1 -1]; "
         "R2 = partialcorr(X, Z); R3 = partialcorr(X, X, Z);");
    EXPECT_NEAR(evalScalar("R2(1,2)"), evalScalar("R3(1,2)"), 1e-12);
    EXPECT_NEAR(evalScalar("R2(2,3)"), evalScalar("R3(2,3)"), 1e-12);
    EXPECT_NEAR(evalScalar("R2(1,3)"), evalScalar("R3(1,3)"), 1e-12);
}

TEST_F(PartialCorrTest, TwoArgVectorVectorScalar)
{
    // x is 5×1, z is 5×1 → output 1×1 (single pair, no other X column).
    eval("x = [1; 3; 5; 7; 9]; z = [2; 1; 3; 4; 6]; "
         "R = partialcorr(x, z);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,2)")), 1);
    EXPECT_NEAR(evalScalar("R(1,1)"), 1.0, 1e-14);
}

// ── partialcorr 'Rows' NaN policy (2026-05-30): 'complete' ───────────
// partialcorr previously accept-and-ignored the 'Rows' NV pair, so NaN
// data NaN-poisoned. 'complete' now applies listwise deletion across all
// positional matrices before computing the partial correlation.
// vs MATLAB R2025b.
TEST_F(PartialCorrTest, RowsCompleteListwise)
{
    eval("Xn = [1 5 2; 2 6 9; 3 NaN 4; 4 8 1; 5 9 6; 6 3 NaN; 7 2 5];");
    eval("Rc = partialcorr(Xn,'Rows','complete');");
    EXPECT_NEAR(evalScalar("Rc(1,1)"),  1.0,                1e-12);
    EXPECT_NEAR(evalScalar("Rc(1,2)"), -0.227122955741229, 1e-9);
    EXPECT_NEAR(evalScalar("Rc(1,3)"),  0.040291148201269, 1e-9);
    EXPECT_NEAR(evalScalar("Rc(2,3)"), -0.046205274725598, 1e-9);
    // 'all' (default) still NaN-poisons.
    EXPECT_TRUE(std::isnan(evalScalar("Ra = partialcorr(Xn); Ra(1,2)")));
    // 'pairwise' is deferred -> clear error.
    EXPECT_THROW(eval("partialcorr(Xn,'Rows','pairwise');"), std::exception);
}
