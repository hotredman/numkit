// libs/builtin/tests/manip_test.cpp
// Phase 5: repmat / fliplr / flipud / rot90 / circshift / tril / triu

#include "dual_engine_fixture.hpp"

using namespace m_test;

class ManipTest : public DualEngineTest
{};

// ── repmat ──────────────────────────────────────────────────

TEST_P(ManipTest, RepmatScalar)
{
    eval("A = repmat(5, 2, 3);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 3u);
    for (size_t i = 0; i < A->numel(); ++i)
        EXPECT_DOUBLE_EQ(A->doubleData()[i], 5.0);
}

TEST_P(ManipTest, RepmatVectorTile)
{
    // repmat([1 2 3], 2, 2) → 2 row tiles × 2 col tiles → 2×6
    eval("A = repmat([1 2 3], 2, 2);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 6u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 2.0);
    EXPECT_DOUBLE_EQ((*A)(0, 2), 3.0);
    EXPECT_DOUBLE_EQ((*A)(0, 3), 1.0);  // tiled
    EXPECT_DOUBLE_EQ((*A)(1, 5), 3.0);
}

TEST_P(ManipTest, RepmatMatrix)
{
    // repmat([1 2; 3 4], 2, 3) → 4×6
    eval("A = repmat([1 2; 3 4], 2, 3);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 4u);
    EXPECT_EQ(cols(*A), 6u);
    // First tile (0..1, 0..1) is [1 2; 3 4]
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 4.0);
    // Mirror tile in (2..3, 4..5)
    EXPECT_DOUBLE_EQ((*A)(2, 4), 1.0);
    EXPECT_DOUBLE_EQ((*A)(3, 5), 4.0);
}

TEST_P(ManipTest, RepmatScalarN)
{
    // repmat(A, n) = repmat(A, n, n)
    eval("A = repmat(7, 3);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 3u);
}

TEST_P(ManipTest, RepmatVectorArg)
{
    eval("A = repmat([1 2; 3 4], [3 2]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 6u);
    EXPECT_EQ(cols(*A), 4u);
}

TEST_P(ManipTest, Repmat3D)
{
    // 2D source tiled into 3D output: repmat(A, 1, 1, 3) → R×C×3 stacked.
    eval("A = repmat([1 2; 3 4], 1, 1, 3);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 2u);
    // numel = 12 (2*2*3); access via linear or 3D
    EXPECT_DOUBLE_EQ(A->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(A->doubleData()[3], 4.0);  // first page
    EXPECT_DOUBLE_EQ(A->doubleData()[4], 1.0);  // second page starts
    EXPECT_DOUBLE_EQ(A->doubleData()[11], 4.0);
}

// ── fliplr / flipud ─────────────────────────────────────────

TEST_P(ManipTest, FliplrRowVector)
{
    eval("v = fliplr([1 2 3 4]);");
    auto *v = getVarPtr("v");
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 1.0);
}

TEST_P(ManipTest, FliplrMatrix)
{
    eval("A = fliplr([1 2 3; 4 5 6]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 3u);
    // each row is reversed
    EXPECT_DOUBLE_EQ((*A)(0, 0), 3.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 2.0);
    EXPECT_DOUBLE_EQ((*A)(0, 2), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 6.0);
}

TEST_P(ManipTest, FlipudColumnVector)
{
    eval("v = flipud([1; 2; 3; 4]);");
    auto *v = getVarPtr("v");
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 1.0);
}

TEST_P(ManipTest, FlipudMatrix)
{
    eval("A = flipud([1 2 3; 4 5 6; 7 8 9]);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 7.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 5.0);
    EXPECT_DOUBLE_EQ((*A)(2, 2), 3.0);
}

// ── rot90 ───────────────────────────────────────────────────

TEST_P(ManipTest, Rot90CCW)
{
    // [1 2 3; 4 5 6] rotated 90° CCW → 3×2 matrix
    // [3 6; 2 5; 1 4]
    eval("A = rot90([1 2 3; 4 5 6]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 2u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 3.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 6.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 2.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 5.0);
    EXPECT_DOUBLE_EQ((*A)(2, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(2, 1), 4.0);
}

TEST_P(ManipTest, Rot90KEquals2)
{
    // 180° rotation
    eval("A = rot90([1 2; 3 4], 2);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 4.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 3.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 2.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 1.0);
}

TEST_P(ManipTest, Rot90KNegative)
{
    // -1 == clockwise == 270°
    eval("A = rot90([1 2 3; 4 5 6], -1);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 2u);
    // Clockwise: column 0 of result = top row of input reversed = [4 1]
    EXPECT_DOUBLE_EQ((*A)(0, 0), 4.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 1.0);
    EXPECT_DOUBLE_EQ((*A)(2, 0), 6.0);
    EXPECT_DOUBLE_EQ((*A)(2, 1), 3.0);
}

TEST_P(ManipTest, Rot90KMod4)
{
    // k=4 == identity
    eval("A = rot90([1 2; 3 4], 4);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 4.0);
}

// ── circshift ──────────────────────────────────────────────

TEST_P(ManipTest, CircshiftVectorPositive)
{
    // shift right by 2: [1 2 3 4 5] → [4 5 1 2 3]
    eval("v = circshift([1 2 3 4 5], 2);");
    auto *v = getVarPtr("v");
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 5.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 3.0);
}

TEST_P(ManipTest, CircshiftVectorNegative)
{
    // shift left by 2: [1 2 3 4 5] → [3 4 5 1 2]
    eval("v = circshift([1 2 3 4 5], -2);");
    auto *v = getVarPtr("v");
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 4.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 2.0);
}

TEST_P(ManipTest, CircshiftWrapsAroundLength)
{
    // shift by 7 on length-5 = shift by 2
    eval("v = circshift([1 2 3 4 5], 7);");
    auto *v = getVarPtr("v");
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 4.0);
}

TEST_P(ManipTest, CircshiftMatrixRowsCols)
{
    // [1 2 3; 4 5 6; 7 8 9] shifted by [1 0]
    // dim=1 shift down by 1: [7 8 9; 1 2 3; 4 5 6]
    eval("A = circshift([1 2 3; 4 5 6; 7 8 9], [1 0]);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 7.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 8.0);
    EXPECT_DOUBLE_EQ((*A)(0, 2), 9.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 1.0);
}

TEST_P(ManipTest, CircshiftMatrixBothDims)
{
    // [1 2; 3 4] shifted by [1 1]: shift rows down by 1, cols right by 1
    // Original:  [1 2;
    //             3 4]
    // Shift down: [3 4; 1 2]
    // Shift right by 1: [4 3; 2 1]
    eval("A = circshift([1 2; 3 4], [1 1]);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 4.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 3.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 2.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 1.0);
}

// ── tril / triu ────────────────────────────────────────────

TEST_P(ManipTest, TrilMainDiagonal)
{
    // tril([1 2 3; 4 5 6; 7 8 9]) — keep main diag and below
    eval("A = tril([1 2 3; 4 5 6; 7 8 9]);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 0.0);  // above main → 0
    EXPECT_DOUBLE_EQ((*A)(0, 2), 0.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 4.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 5.0);
    EXPECT_DOUBLE_EQ((*A)(1, 2), 0.0);
    EXPECT_DOUBLE_EQ((*A)(2, 0), 7.0);
    EXPECT_DOUBLE_EQ((*A)(2, 1), 8.0);
    EXPECT_DOUBLE_EQ((*A)(2, 2), 9.0);
}

TEST_P(ManipTest, TrilWithKPositive)
{
    // tril(A, 1) keeps main + first super-diag
    eval("A = tril([1 2 3; 4 5 6; 7 8 9], 1);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 2.0);  // super-diag kept
    EXPECT_DOUBLE_EQ((*A)(0, 2), 0.0);
    EXPECT_DOUBLE_EQ((*A)(1, 2), 6.0);
}

TEST_P(ManipTest, TrilWithKNegative)
{
    // tril(A, -1) keeps strictly below main diag
    eval("A = tril([1 2 3; 4 5 6; 7 8 9], -1);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 0.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 4.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 0.0);
    EXPECT_DOUBLE_EQ((*A)(2, 0), 7.0);
    EXPECT_DOUBLE_EQ((*A)(2, 1), 8.0);
    EXPECT_DOUBLE_EQ((*A)(2, 2), 0.0);
}

TEST_P(ManipTest, TriuMainDiagonal)
{
    eval("A = triu([1 2 3; 4 5 6; 7 8 9]);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 2.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 0.0);  // below main → 0
    EXPECT_DOUBLE_EQ((*A)(2, 0), 0.0);
    EXPECT_DOUBLE_EQ((*A)(2, 1), 0.0);
    EXPECT_DOUBLE_EQ((*A)(2, 2), 9.0);
}

TEST_P(ManipTest, TriuWithK)
{
    // triu(A, 1) — strictly above main diag
    eval("A = triu([1 2 3; 4 5 6; 7 8 9], 1);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 0.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 2.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 0.0);
    EXPECT_DOUBLE_EQ((*A)(1, 2), 6.0);
}

TEST_P(ManipTest, TriuRectangular)
{
    // 3×4 matrix
    eval("A = triu([1 2 3 4; 5 6 7 8; 9 10 11 12]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 4u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(2, 0), 0.0);
    EXPECT_DOUBLE_EQ((*A)(2, 3), 12.0);
    EXPECT_DOUBLE_EQ((*A)(2, 1), 0.0);
}

// ── 3D coverage for rot90 / tril / triu ──────────────────────
//
// Pre-2026-04-23 these threw on 3D inputs. New implementations apply
// the operation per page (preserving the page count, swapping rows/cols
// for rot90 odd-k as expected for 2D rotation).

TEST_P(ManipTest, Rot90On3DRotatesEachPage)
{
    // Two 2×3 pages: page 1 = [1 2 3; 4 5 6], page 2 = [10 20 30; 40 50 60]
    eval("A = zeros(2, 3, 2); "
         "A(:,:,1) = [1 2 3; 4 5 6]; "
         "A(:,:,2) = [10 20 30; 40 50 60]; "
         "B = rot90(A);");
    auto *B = getVarPtr("B");
    // rot90 on 2x3 page → 3x2 page; pages preserved.
    EXPECT_EQ(rows(*B), 3u);
    EXPECT_EQ(cols(*B), 2u);
    EXPECT_TRUE(B->dims().is3D());
    EXPECT_EQ(B->dims().pages(), 2u);
    // Page 1: rot90([1 2 3; 4 5 6]) =
    //   [3 6;
    //    2 5;
    //    1 4]
    EXPECT_DOUBLE_EQ((*B)(0, 0, 0), 3.0);
    EXPECT_DOUBLE_EQ((*B)(0, 1, 0), 6.0);
    EXPECT_DOUBLE_EQ((*B)(2, 0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*B)(2, 1, 0), 4.0);
    // Page 2: rot90([10 20 30; 40 50 60]) = [30 60; 20 50; 10 40]
    EXPECT_DOUBLE_EQ((*B)(0, 0, 1), 30.0);
    EXPECT_DOUBLE_EQ((*B)(2, 0, 1), 10.0);
    EXPECT_DOUBLE_EQ((*B)(2, 1, 1), 40.0);
}

TEST_P(ManipTest, Rot90KEquals2On3DPreservesShape)
{
    eval("A = zeros(2, 3, 2); "
         "A(:,:,1) = [1 2 3; 4 5 6]; "
         "A(:,:,2) = [10 20 30; 40 50 60]; "
         "B = rot90(A, 2);");
    auto *B = getVarPtr("B");
    // rot180 keeps shape (2, 3, 2)
    EXPECT_EQ(rows(*B), 2u);
    EXPECT_EQ(cols(*B), 3u);
    EXPECT_EQ(B->dims().pages(), 2u);
    // Page 1: rot180([1 2 3; 4 5 6]) = [6 5 4; 3 2 1]
    EXPECT_DOUBLE_EQ((*B)(0, 0, 0), 6.0);
    EXPECT_DOUBLE_EQ((*B)(0, 2, 0), 4.0);
    EXPECT_DOUBLE_EQ((*B)(1, 0, 0), 3.0);
    EXPECT_DOUBLE_EQ((*B)(1, 2, 0), 1.0);
    // Page 2 spot-check
    EXPECT_DOUBLE_EQ((*B)(0, 0, 1), 60.0);
    EXPECT_DOUBLE_EQ((*B)(1, 2, 1), 10.0);
}

TEST_P(ManipTest, TrilOn3DAppliesPerPage)
{
    eval("A = zeros(3, 3, 2); "
         "A(:,:,1) = [1 2 3; 4 5 6; 7 8 9]; "
         "A(:,:,2) = [10 20 30; 40 50 60; 70 80 90]; "
         "B = tril(A);");
    auto *B = getVarPtr("B");
    EXPECT_EQ(rows(*B), 3u);
    EXPECT_EQ(cols(*B), 3u);
    EXPECT_EQ(B->dims().pages(), 2u);
    // Page 1: lower triangular of [1 2 3; 4 5 6; 7 8 9]
    EXPECT_DOUBLE_EQ((*B)(0, 0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*B)(0, 1, 0), 0.0); // above diag
    EXPECT_DOUBLE_EQ((*B)(0, 2, 0), 0.0);
    EXPECT_DOUBLE_EQ((*B)(2, 0, 0), 7.0);
    EXPECT_DOUBLE_EQ((*B)(2, 2, 0), 9.0);
    // Page 2: spot
    EXPECT_DOUBLE_EQ((*B)(1, 0, 1), 40.0);
    EXPECT_DOUBLE_EQ((*B)(0, 2, 1), 0.0);
}

TEST_P(ManipTest, TriuOn3DAppliesPerPage)
{
    eval("A = zeros(3, 3, 2); "
         "A(:,:,1) = [1 2 3; 4 5 6; 7 8 9]; "
         "A(:,:,2) = [10 20 30; 40 50 60; 70 80 90]; "
         "B = triu(A);");
    auto *B = getVarPtr("B");
    EXPECT_EQ(rows(*B), 3u);
    EXPECT_EQ(B->dims().pages(), 2u);
    // Page 1: upper triangular
    EXPECT_DOUBLE_EQ((*B)(0, 0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*B)(0, 2, 0), 3.0);
    EXPECT_DOUBLE_EQ((*B)(2, 0, 0), 0.0); // below diag
    EXPECT_DOUBLE_EQ((*B)(1, 0, 0), 0.0);
    // Page 2: spot
    EXPECT_DOUBLE_EQ((*B)(0, 2, 1), 30.0);
    EXPECT_DOUBLE_EQ((*B)(1, 0, 1), 0.0);
}

INSTANTIATE_DUAL(ManipTest);
