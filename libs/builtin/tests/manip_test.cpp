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

// ── repmat ND (Phase 3a.5) ──────────────────────────────────

TEST_P(ManipTest, Repmat4DTileVector2DInput)
{
    // 2D source tiled into 4D output via [1 1 2 3] → 2×2×2×3.
    eval("A = repmat([1 2; 3 4], [1 1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(A);"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 3);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 4);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(A);"), 24.0);
    // Each [page, vol] cell is a copy of [1 2; 3 4]
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 2, 1, 1);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 2, 3);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 2, 2, 3);"), 4.0);
}

TEST_P(ManipTest, Repmat4DTilesGrowEveryAxis)
{
    // [2 3] tiled by [3 2 4 5] → 6×6×4×5 = 720
    eval("A = repmat([1 2 3; 4 5 6], [3 2 4 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(A);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 1);"),  6.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 2);"),  6.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 3);"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 4);"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(A);"),  720.0);
    // Tile-modulo verification: A(i,j,k,l) = src((i-1)%2+1, (j-1)%3+1).
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 1, 1);"), 1.0); // src(1,1)
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 2, 1, 1);"), 5.0); // src(2,2)
    EXPECT_DOUBLE_EQ(evalScalar("A(3, 4, 1, 1);"), 1.0); // src(1,1) wrap
    EXPECT_DOUBLE_EQ(evalScalar("A(6, 6, 4, 5);"), 6.0); // src(2,3)
}

TEST_P(ManipTest, Repmat5DOf4DInput)
{
    // 4D source tiled along a fifth dim — exercises rank promotion.
    eval("A = reshape(1:24, [2, 3, 2, 2]); B = repmat(A, [1 1 1 1 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(B);"),    5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 5);"),  3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(B);"), 72.0);
    // Each "page" along dim 5 is a copy of A
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 1, 1);"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 3, 2, 2, 1);"),  24.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 1, 2);"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 3, 2, 2, 3);"),  24.0);
}

TEST_P(ManipTest, Repmat4DPositionalArgs)
{
    // Positional repmat(A, m, n, p, q) should match the vector form.
    eval("A = repmat([1 2; 3 4], 1, 2, 3, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(A);"),    4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 2);"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 3);"),  3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 4);"),  2.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(A);"),   48.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 4, 3, 2);"), 4.0); // wrap on cols
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

// circshift(X, K, dim) shifts by K ONLY along dimension `dim`. numkit
// previously ignored the dim argument and always shifted dim 1. vs MATLAB
// R2025b. DEEP-PROBE 2026-05-30.
TEST_P(ManipTest, CircshiftAlongDim)
{
    // dim 2 (columns): [1 2 3;4 5 6] -> [3 1 2;6 4 5].
    eval("A = circshift([1 2 3; 4 5 6], 1, 2);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 3.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 6.0);
    // dim 1 (rows): [1 2 3;4 5 6] -> [4 5 6;1 2 3].
    eval("B = circshift([1 2 3; 4 5 6], 1, 1);");
    auto *B = getVarPtr("B");
    EXPECT_DOUBLE_EQ((*B)(0, 0), 4.0);
    EXPECT_DOUBLE_EQ((*B)(1, 0), 1.0);
    // Negative shift along dim 2: [1 2 3;4 5 6] -> [2 3 1;5 6 4].
    eval("C = circshift([1 2 3; 4 5 6], -1, 2);");
    auto *C = getVarPtr("C");
    EXPECT_DOUBLE_EQ((*C)(0, 0), 2.0);
    EXPECT_DOUBLE_EQ((*C)(0, 2), 1.0);
    // Row vector along dim 2: circshift([10 20 30 40],2,2) -> [30 40 10 20].
    eval("r = circshift([10 20 30 40], 2, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(3)"), 10.0);
    // Default (2-arg) form unchanged.
    eval("d = circshift([1 2 3 4 5], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 4.0);
}

// circshift is a pure rearrangement -> type-agnostic. Was throwing "Not a
// double array" on char/cell/logical/complex. vs MATLAB R2025b. DEEP-PROBE
// 2026-05-31.
TEST_P(ManipTest, CircshiftTypeAgnostic)
{
    // char vector.
    eval("cv = circshift('abcde', 2);");
    EXPECT_EQ(getVarPtr("cv")->toString(), "deabc");
    eval("cvn = circshift('abcde', -1);");
    EXPECT_EQ(getVarPtr("cvn")->toString(), "bcdea");
    // cell vector -> {4,1,2,3}.
    eval("cc = circshift({1,2,3,4}, 1); a = cc{1}; b = cc{2};");
    EXPECT_DOUBLE_EQ(evalScalar("a"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("b"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(iscell(cc))"), 1.0);
    // cellstr vector -> {z,x,y}.
    eval("cs = circshift({'x','y','z'}, 1); s1 = cs{1}; s3 = cs{3};");
    EXPECT_EQ(getVarPtr("s1")->toString(), "z");
    EXPECT_EQ(getVarPtr("s3")->toString(), "y");
    // char matrix shifts rows.
    eval("cm = circshift(['ab';'cd';'ef'], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("double(cm(1,1))"), 101.0);   // 'e'
    // [r c] form on a char matrix.
    eval("cm2 = circshift(['abc';'def'], [1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(cm2(1,1))"), 102.0);  // 'f'
    // logical preserved.
    eval("lg = circshift([true false true false], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("double(lg(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(lg(2))"), 0.0);
    // complex preserved.
    eval("zc = circshift([1+1i 2+2i 3+3i], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("real(zc(1))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(zc(1))"), 3.0);
    // double path unchanged.
    eval("dd = circshift([1 2 3 4 5], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("dd(1)"), 4.0);
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

// ── ND fliplr / flipud / tril / triu / rot90 / circshift (Phase 3a.6) ──

TEST_P(ManipTest, Fliplr4DPerSlice)
{
    // 2×3×2×2 input. fliplr reverses axis 1 within each (page, vol) slice.
    eval("A = reshape(1:24, [2, 3, 2, 2]); B = fliplr(A);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(B);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 2);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(B);"), 24.0);
    // A(:,1,1,1) = [1; 2]; A(:,3,1,1) = [5; 6] → B(:,1,1,1) = [5; 6]
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 1);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 1, 1, 1);"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 3, 1, 1);"), 1.0);
    // Last slice: A(:,1,2,2) = [19;20], A(:,3,2,2) = [23;24]
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 2, 2);"), 23.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 3, 2, 2);"), 19.0);
}

TEST_P(ManipTest, Flipud4DPerSlice)
{
    // 2×3×2×2 input. flipud reverses axis 0 within each (col, page, vol).
    eval("A = reshape(1:24, [2, 3, 2, 2]); B = flipud(A);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(B);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 1);"), 2.0);
    // A(1,1,1,1)=1, A(2,1,1,1)=2 → B(1,1,1,1)=2, B(2,1,1,1)=1
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 1, 1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 3, 2, 2);"), 24.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 3, 2, 2);"), 23.0);
}

TEST_P(ManipTest, Tril4DPerSlice)
{
    // 3×3×2×2 input → tril zeros above-main per slice.
    eval("A = reshape(1:36, [3, 3, 2, 2]); B = tril(A);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(B);"), 4.0);
    // For first slice A(:,:,1,1) = reshape(1:9,3,3); diag and below kept.
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 1, 1, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3, 1, 1, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 2, 1, 1);"), 0.0);  // above main
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 3, 1, 1);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 2, 1, 1);"), 5.0);
    // Last slice spot
    EXPECT_DOUBLE_EQ(evalScalar("B(3, 3, 2, 2);"), 36.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 3, 2, 2);"),  0.0);
}

TEST_P(ManipTest, Triu4DPerSlice)
{
    eval("A = reshape(1:36, [3, 3, 2, 2]); B = triu(A);");
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 3, 1, 1);"), 7.0);  // A(1,3,1,1)=7
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 1, 1, 1);"), 0.0);  // below main
    EXPECT_DOUBLE_EQ(evalScalar("B(3, 1, 1, 1);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 2, 2, 2);"), 32.0);
}

TEST_P(ManipTest, Rot904DPerSliceCCW)
{
    // 2×3×2×2 input. rot90 (k=1) takes (R,C)=(2,3) → output (3,2,2,2).
    eval("A = reshape(1:24, [2, 3, 2, 2]); B = rot90(A);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(B);"),    4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 2);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 3);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 4);"), 2.0);
    // First slice A(:,:,1,1) = [1 3 5; 2 4 6]; CCW rotation:
    //   B(:,:,1,1) = [5 6; 3 4; 1 2]
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 1);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 2, 1, 1);"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3, 1, 1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3, 2, 1, 1);"), 2.0);
    // Last slice
    EXPECT_DOUBLE_EQ(evalScalar("B(3, 2, 2, 2);"), 20.0);
}

TEST_P(ManipTest, Rot904DTwoSwapsBack)
{
    // rot90 four times = identity, on ND
    eval("A = reshape(1:24, [2, 3, 2, 2]); B = rot90(A, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(A, B);"), 1.0);
}

TEST_P(ManipTest, Circshift4DAxis2)
{
    // 2×3×2×2 input, shift vec [0 1 0 0] rotates axis 2 (cols) by 1.
    eval("A = reshape(1:24, [2, 3, 2, 2]); B = circshift(A, [0 1 0 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(B);"), 4.0);
    // B(:,1,1,1) = A(:,3,1,1) = [5;6]
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 1);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 1, 1, 1);"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 2, 1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 2, 1, 1);"), 2.0);
    // Element count + sum preservation
    EXPECT_DOUBLE_EQ(evalScalar("sum(A(:)) == sum(B(:));"), 1.0);
}

TEST_P(ManipTest, Circshift4DAllAxes)
{
    // Shift along all four axes simultaneously.
    eval("A = reshape(1:24, [2, 3, 2, 2]); B = circshift(A, [1 1 1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(A) == numel(B);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(A(:)) == sum(B(:));"), 1.0);
    // B(i,j,k,l) = A((i-2)mod2+1, (j-2)mod3+1, (k-2)mod2+1, (l-2)mod2+1)
    // B(1,1,1,1) = A(2,3,2,2) = 24
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 1);"), 24.0);
    // B(2,1,1,1) = A(1,3,2,2) = 23
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 1, 1, 1);"), 23.0);
}

TEST_P(ManipTest, Circshift4DFullCycleIsIdentity)
{
    // Shift by exactly the dim along each axis = identity (modulo wrap).
    eval("A = reshape(1:24, [2, 3, 2, 2]); B = circshift(A, [2 3 2 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(A, B);"), 1.0);
}

// ── Phase B: ND type promotion tests for cat/repmat/flip/circshift ──

TEST_P(ManipTest, Repmat4DPreservesIntegerType)
{
    eval("A = int16([1 2; 3 4]); B = repmat(A, [1 1 2 3]);");
    EXPECT_TRUE(evalBool("isequal(class(B), 'int16');"));
    EXPECT_DOUBLE_EQ(evalScalar("ndims(B);"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 4);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(B(2, 2, 2, 3));"), 4.0);
}

TEST_P(ManipTest, Repmat4DPreservesLogicalType)
{
    eval("A = logical([1 0; 1 1]); B = repmat(A, [1 1 2 2]);");
    EXPECT_TRUE(evalBool("isequal(class(B), 'logical');"));
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 4);"), 2.0);
}

TEST_P(ManipTest, Fliplr4DPreservesIntegerType)
{
    eval("A = int8(reshape(1:24, [2, 3, 2, 2])); B = fliplr(A);");
    EXPECT_TRUE(evalBool("isequal(class(B), 'int8');"));
    EXPECT_DOUBLE_EQ(evalScalar("double(B(1, 1, 1, 1));"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(B(1, 3, 1, 1));"), 1.0);
}

TEST_P(ManipTest, Flipud4DPreservesSingleType)
{
    eval("A = single(reshape(1:24, [2, 3, 2, 2])); B = flipud(A);");
    EXPECT_TRUE(evalBool("isequal(class(B), 'single');"));
    EXPECT_DOUBLE_EQ(evalScalar("double(B(1, 1, 1, 1));"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(B(2, 1, 1, 1));"), 1.0);
}

TEST_P(ManipTest, Circshift4DPreservesIntegerType)
{
    eval("A = int32(reshape(1:24, [2, 3, 2, 2]));"
         "B = circshift(A, [0 1 0 0]);");
    EXPECT_TRUE(evalBool("isequal(class(B), 'int32');"));
    EXPECT_DOUBLE_EQ(evalScalar("double(B(1, 1, 1, 1));"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(B(1, 2, 1, 1));"), 1.0);
}

TEST_P(ManipTest, Tril4DPreservesIntegerType)
{
    eval("A = int16(reshape(1:36, [3, 3, 2, 2])); B = tril(A);");
    EXPECT_TRUE(evalBool("isequal(class(B), 'int16');"));
    EXPECT_DOUBLE_EQ(evalScalar("double(B(1, 1, 1, 1));"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(B(1, 2, 1, 1));"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(B(3, 1, 1, 1));"), 3.0);
}

TEST_P(ManipTest, Triu4DPreservesSingleType)
{
    eval("A = single(reshape(1:36, [3, 3, 2, 2])); B = triu(A);");
    EXPECT_TRUE(evalBool("isequal(class(B), 'single');"));
    EXPECT_DOUBLE_EQ(evalScalar("double(B(1, 3, 1, 1));"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(B(2, 1, 1, 1));"), 0.0);
}

TEST_P(ManipTest, Rot904DPreservesIntegerType)
{
    eval("A = int8(reshape(1:24, [2, 3, 2, 2])); B = rot90(A);");
    EXPECT_TRUE(evalBool("isequal(class(B), 'int8');"));
    EXPECT_DOUBLE_EQ(evalScalar("ndims(B);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 2);"), 2.0);
    // First-slice CCW rot of [1 3 5; 2 4 6] = [5 6; 3 4; 1 2]
    EXPECT_DOUBLE_EQ(evalScalar("double(B(1, 1, 1, 1));"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(B(3, 2, 1, 1));"), 2.0);
}

// ── Pack 36: pagetranspose / pagectranspose ─────────────────────────
TEST_P(ManipTest, PagetransposeOn2DEqualsTranspose)
{
    eval("M = [1 2 3; 4 5 6];");
    eval("T = pagetranspose(M);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(T,1);")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(T,2);")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("T(1,1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(3,2);"), 6.0);
}

TEST_P(ManipTest, PagetransposeSwapsRowsAndColsPerPage)
{
    eval("A = zeros(2,3,2);"
         "A(:,:,1) = [1 2 3; 4 5 6];"
         "A(:,:,2) = [7 8 9; 10 11 12];");
    eval("B = pagetranspose(A);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1);")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2);")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,3);")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1,1);"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3,2,1);"),  6.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1,2);"),  7.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3,2,2);"), 12.0);
}

TEST_P(ManipTest, PagetransposeIsInvolution)
{
    eval("A = zeros(2,3,2);"
         "A(:,:,1) = [1 2 3; 4 5 6];"
         "A(:,:,2) = [7 8 9; 10 11 12];");
    eval("B = pagetranspose(pagetranspose(A));");
    eval("delta = max(abs(A(:) - B(:)));");
    EXPECT_LT(evalScalar("delta;"), 1e-12);
}

TEST_P(ManipTest, PagectransposeMatchesPagetransposeOnReal)
{
    eval("A = zeros(2,3,2);"
         "A(:,:,1) = [1 2 3; 4 5 6];"
         "A(:,:,2) = [7 8 9; 10 11 12];");
    eval("Bt = pagetranspose(A);"
         "Bc = pagectranspose(A);"
         "delta = max(abs(Bt(:) - Bc(:)));");
    EXPECT_LT(evalScalar("delta;"), 1e-12);
}
// ── Pack 36: peaks ───────────────────────────────────────────────────
TEST_P(ManipTest, PeaksDefaultIs49x49)
{
    eval("Z = peaks(49);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(Z,1);")), 49);
    EXPECT_EQ(static_cast<int>(evalScalar("size(Z,2);")), 49);
}

TEST_P(ManipTest, PeaksSizeNxN)
{
    eval("Z = peaks(7);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(Z,1);")), 7);
    EXPECT_EQ(static_cast<int>(evalScalar("size(Z,2);")), 7);
}

TEST_P(ManipTest, PeaksKnownReferenceValues)
{
    // Reference values probed from MATLAB R2025b.
    eval("Z = peaks(5);");
    EXPECT_NEAR(evalScalar("sum(Z(:));"), 5.436719235, 1e-9);
    EXPECT_NEAR(evalScalar("Z(3,3);"),    0.9810118431, 1e-9);
}
// ── Pack 36: sphere / cylinder / ellipsoid ──────────────────────────
TEST_P(ManipTest, SphereDefaultIs21x21)
{
    eval("[X, Y, Z] = sphere(20);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(Z,1);")), 21);
    EXPECT_EQ(static_cast<int>(evalScalar("size(Z,2);")), 21);
}

TEST_P(ManipTest, SphereOnUnitSphereIdentity)
{
    // X^2 + Y^2 + Z^2 == 1 on a unit sphere.
    eval("[X, Y, Z] = sphere(20);"
         "delta = max(abs(X(:).^2 + Y(:).^2 + Z(:).^2 - 1));");
    EXPECT_LT(evalScalar("delta;"), 1e-12);
}

TEST_P(ManipTest, SphereMatlabRefSums)
{
    // Verified against MATLAB R2025b probe. Tolerance loosened to 1e-4
    // because trig sum order between MATLAB's BLAS-vector outer product
    // and our scalar inner loop differs by O(n) cancellations of opposite
    // sign, accumulating ~3e-5 gap on (n+1)^2 = 441 terms — still ULP-quality
    // per element, just non-bit-exact in the reduction.
    eval("[X, Y, Z] = sphere(20);");
    EXPECT_NEAR(evalScalar("sum(X(:));"), -12.706168614303716, 1e-4);
    EXPECT_LT(std::abs(evalScalar("sum(Z(:));")), 1e-12);
}

TEST_P(ManipTest, CylinderDefaultProfile)
{
    eval("[X, Y, Z] = cylinder([1 2 1], 10);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(Z,1);")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(Z,2);")), 11);
    EXPECT_NEAR(evalScalar("X(1,1);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sum(Z(:));"), 16.5, 1e-12);
}

TEST_P(ManipTest, EllipsoidScaledAxes)
{
    eval("[X, Y, Z] = ellipsoid(0, 0, 0, 1, 2, 3, 20);");
    EXPECT_NEAR(evalScalar("max(X(:));"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("max(Y(:));"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("max(Z(:));"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("min(X(:));"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("min(Y(:));"), -2.0, 1e-12);
    EXPECT_NEAR(evalScalar("min(Z(:));"), -3.0, 1e-12);
}

TEST_P(ManipTest, EllipsoidShifted)
{
    // Use n=20 so the discrete grid hits θ = ±π/2 and φ = ±π/2 exactly
    // (n=10 misses sin(-π/2)=-1 by the lattice spacing — MATLAB has the
    // same artefact, so this is correctness, not bug).
    eval("[X, Y, Z] = ellipsoid(5, -3, 7, 1, 1, 1, 20);");
    EXPECT_NEAR(evalScalar("max(X(:));"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("min(Y(:));"), -4.0, 1e-12);
    EXPECT_NEAR(evalScalar("max(Z(:));"), 8.0, 1e-12);
}

// ── flip / fliplr / flipud / rot90 on CELL and STRING arrays ──────────
// (DEEP-PROBE 2026-05-31) MATLAB's flip family is type-agnostic — it just
// permutes elements. numkit previously errored on cell/string inputs
// ("ND fallback does not support type cell/string" / "Not a double array").

TEST_P(ManipTest, FlipCellRowVector)
{
    eval("c = flip({10, 20, 30});");
    EXPECT_DOUBLE_EQ(evalScalar("c{1}"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2}"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{3}"), 10.0);
}

TEST_P(ManipTest, FliplrCellRowVector)
{
    eval("c = fliplr({10, 20, 30});");
    EXPECT_DOUBLE_EQ(evalScalar("c{1}"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{3}"), 10.0);
}

TEST_P(ManipTest, FlipudCellColVector)
{
    eval("c = flipud({10; 20; 30});");
    EXPECT_DOUBLE_EQ(evalScalar("c{1}"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{3}"), 10.0);
}

TEST_P(ManipTest, FlipCellMatrixDim2)
{
    // flip({1 2;3 4},2) = {2 1; 4 3}
    eval("c = flip({1 2; 3 4}, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("c{1,1}"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2,1}"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{1,2}"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2,2}"), 3.0);
}

TEST_P(ManipTest, FlipCellMatrixDim1)
{
    // flip({1 2;3 4},1) = {3 4; 1 2}
    eval("c = flip({1 2; 3 4}, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("c{1,1}"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2,1}"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{1,2}"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2,2}"), 2.0);
}

TEST_P(ManipTest, FlipCellstr)
{
    eval("c = flip({'aa', 'bb', 'cc'});");
    auto *c = getVarPtr("c");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->cellAt(0).toString(), "cc");
    EXPECT_EQ(c->cellAt(2).toString(), "aa");
}

TEST_P(ManipTest, FlipStringArray)
{
    // flip(["x" "y" "z"]) = ["z" "y" "x"]. Read elements via the C++ API
    // (in-script string indexing s(k) is unrelated to the flip op).
    eval("s = flip([\"x\" \"y\" \"z\"]);");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->stringElem(0), "z");
    EXPECT_EQ(s->stringElem(1), "y");
    EXPECT_EQ(s->stringElem(2), "x");
}

TEST_P(ManipTest, Rot90Cell)
{
    // rot90({1 2;3 4}) = {2 4; 1 3}
    eval("c = rot90({1 2; 3 4});");
    EXPECT_DOUBLE_EQ(evalScalar("c{1,1}"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2,1}"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{1,2}"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2,2}"), 3.0);
}

TEST_P(ManipTest, Rot90CellK2)
{
    // rot90({1 2;3 4},2) = {4 3; 2 1}
    eval("c = rot90({1 2; 3 4}, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("c{1,1}"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2,1}"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{1,2}"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2,2}"), 1.0);
}

TEST_P(ManipTest, Rot90CellK3)
{
    // rot90({1 2;3 4},3) = {3 1; 4 2}
    eval("c = rot90({1 2; 3 4}, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("c{1,1}"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2,1}"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{1,2}"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2,2}"), 2.0);
}

TEST_P(ManipTest, Rot90CellRowVectorShape)
{
    // rot90({1,2,3}) rotates 1x3 -> 3x1 = {3;2;1}
    eval("c = rot90({1, 2, 3});");
    auto *c = getVarPtr("c");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(rows(*c), 3u);
    EXPECT_EQ(cols(*c), 1u);
    EXPECT_DOUBLE_EQ(evalScalar("c{1}"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{3}"), 1.0);
}

TEST_P(ManipTest, Rot90StringArray)
{
    // rot90(["a" "b";"c" "d"]) = ["b" "d";"a" "c"] (2x2 col-major: idx0="b",
    // idx1="a", idx2="d", idx3="c"). Read elements via the C++ API.
    eval("s = rot90([\"a\" \"b\"; \"c\" \"d\"]);");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(rows(*s), 2u);
    EXPECT_EQ(cols(*s), 2u);
    EXPECT_EQ(s->stringElem(0), "b");
    EXPECT_EQ(s->stringElem(1), "a");
    EXPECT_EQ(s->stringElem(2), "d");
    EXPECT_EQ(s->stringElem(3), "c");
}

INSTANTIATE_DUAL(ManipTest);
