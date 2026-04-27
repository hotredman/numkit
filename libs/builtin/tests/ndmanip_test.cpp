// libs/builtin/tests/ndmanip_test.cpp
// Phase 6: permute / ipermute / squeeze / cat / blkdiag

#include "dual_engine_fixture.hpp"

using namespace m_test;

class NDManipTest : public DualEngineTest
{};

// ── permute ─────────────────────────────────────────────────

TEST_P(NDManipTest, PermuteIdentity2D)
{
    // permute([1 2; 3 4], [1 2]) = same matrix
    eval("A = permute([1 2; 3 4], [1 2]);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 2.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 3.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 4.0);
}

TEST_P(NDManipTest, Permute2DTransposeEquivalent)
{
    // permute(A, [2 1]) == A.'
    eval("A = permute([1 2 3; 4 5 6], [2 1]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 2u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 4.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 2.0);
    EXPECT_DOUBLE_EQ((*A)(2, 1), 6.0);
}

TEST_P(NDManipTest, Permute3DSwapPagesAndCols)
{
    // 2×3×2 → permute [1 3 2] → 2×2×3
    eval("A = zeros(2, 3, 2); "
         "A(:,:,1) = [1 2 3; 4 5 6]; "
         "A(:,:,2) = [10 20 30; 40 50 60]; "
         "B = permute(A, [1 3 2]);");
    auto *B = getVarPtr("B");
    EXPECT_EQ(rows(*B), 2u);
    EXPECT_EQ(cols(*B), 2u);
    // Output dims: B(rows=A.rows, cols=A.pages, pages=A.cols)
    // B(i, j, k) = A(i, k, j)
    // B(0,0,0) = A(0,0,0) = 1
    // B(0,1,0) = A(0,0,1) = 10
    // B(1,0,2) = A(1,2,0) = 6
    // B(1,1,2) = A(1,2,1) = 60
    EXPECT_DOUBLE_EQ((*B)(0, 0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*B)(0, 1, 0), 10.0);
    EXPECT_DOUBLE_EQ((*B)(1, 0, 2), 6.0);
    EXPECT_DOUBLE_EQ((*B)(1, 1, 2), 60.0);
}

TEST_P(NDManipTest, IpermuteRoundTrip)
{
    // ipermute(permute(A, p), p) == A
    eval("A = zeros(2, 3, 2); "
         "A(:,:,1) = [1 2 3; 4 5 6]; "
         "A(:,:,2) = [7 8 9; 10 11 12]; "
         "B = ipermute(permute(A, [3 1 2]), [3 1 2]);");
    auto *A = getVarPtr("A");
    auto *B = getVarPtr("B");
    EXPECT_EQ(B->numel(), A->numel());
    for (size_t i = 0; i < A->numel(); ++i)
        EXPECT_DOUBLE_EQ(B->doubleData()[i], A->doubleData()[i]);
}

TEST_P(NDManipTest, PermuteBadPermThrows)
{
    EXPECT_THROW(eval("permute([1 2; 3 4], [1 1]);"), std::runtime_error);
    EXPECT_THROW(eval("permute([1 2; 3 4], [3 1]);"), std::runtime_error);
}

// ── squeeze ─────────────────────────────────────────────────

TEST_P(NDManipTest, Squeeze3DLeadingSingleton)
{
    // 1×3×4 → 3×4
    eval("A = zeros(1, 3, 4); "
         "A(1,:,1) = [1 2 3]; "
         "A(1,:,2) = [4 5 6]; "
         "A(1,:,3) = [7 8 9]; "
         "A(1,:,4) = [10 11 12]; "
         "B = squeeze(A);");
    auto *B = getVarPtr("B");
    EXPECT_EQ(rows(*B), 3u);
    EXPECT_EQ(cols(*B), 4u);
}

TEST_P(NDManipTest, Squeeze3DMiddleSingleton)
{
    // 3×1×4 → 3×4
    eval("A = zeros(3, 1, 4); "
         "A(:,1,1) = [1; 2; 3]; "
         "A(:,1,2) = [4; 5; 6]; "
         "A(:,1,3) = [7; 8; 9]; "
         "A(:,1,4) = [10; 11; 12]; "
         "B = squeeze(A);");
    auto *B = getVarPtr("B");
    EXPECT_EQ(rows(*B), 3u);
    EXPECT_EQ(cols(*B), 4u);
}

TEST_P(NDManipTest, Squeeze2DNoOp)
{
    eval("A = squeeze([1 2; 3 4]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 2u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 4.0);
}

TEST_P(NDManipTest, Squeeze3DAllNonSingleton)
{
    // 2×3×2 — no singletons, no-op
    eval("A = zeros(2, 3, 2); A(:,:,1) = [1 2 3; 4 5 6]; B = squeeze(A);");
    auto *B = getVarPtr("B");
    EXPECT_EQ(rows(*B), 2u);
    EXPECT_EQ(cols(*B), 3u);
}

// ── cat ─────────────────────────────────────────────────────

TEST_P(NDManipTest, CatDim1IsVertcat)
{
    eval("A = cat(1, [1 2 3], [4 5 6]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 3u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 2), 6.0);
}

TEST_P(NDManipTest, CatDim2IsHorzcat)
{
    eval("A = cat(2, [1; 2; 3], [4; 5; 6]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 2u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(2, 1), 6.0);
}

TEST_P(NDManipTest, CatDim3StackPages)
{
    // Two 2×2 matrices stacked into a 2×2×2 array.
    eval("A = cat(3, [1 2; 3 4], [5 6; 7 8]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 2u);
    EXPECT_DOUBLE_EQ((*A)(0, 0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1, 0), 4.0);
    EXPECT_DOUBLE_EQ((*A)(0, 0, 1), 5.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1, 1), 8.0);
}

TEST_P(NDManipTest, CatDim3ThreeMatrices)
{
    eval("A = cat(3, [1 2; 3 4], [5 6; 7 8], [9 10; 11 12]);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(0, 0, 1), 5.0);
    EXPECT_DOUBLE_EQ((*A)(0, 0, 2), 9.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1, 2), 12.0);
}

TEST_P(NDManipTest, CatDim3MismatchThrows)
{
    EXPECT_THROW(eval("cat(3, [1 2; 3 4], [1 2 3; 4 5 6]);"),
                 std::runtime_error);
}

TEST_P(NDManipTest, CatBadDimThrows)
{
    EXPECT_THROW(eval("cat(0, [1 2], [3 4]);"), std::runtime_error);
    EXPECT_THROW(eval("cat(4, [1 2], [3 4]);"), std::runtime_error);
}

// ── blkdiag ─────────────────────────────────────────────────

TEST_P(NDManipTest, BlkdiagTwoMatrices)
{
    // [1 2;3 4] block-diag with [5 6;7 8] → 4×4
    eval("A = blkdiag([1 2; 3 4], [5 6; 7 8]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 4u);
    EXPECT_EQ(cols(*A), 4u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 4.0);
    EXPECT_DOUBLE_EQ((*A)(2, 2), 5.0);
    EXPECT_DOUBLE_EQ((*A)(3, 3), 8.0);
    // Off-diagonal blocks must be zero
    EXPECT_DOUBLE_EQ((*A)(0, 2), 0.0);
    EXPECT_DOUBLE_EQ((*A)(0, 3), 0.0);
    EXPECT_DOUBLE_EQ((*A)(2, 0), 0.0);
    EXPECT_DOUBLE_EQ((*A)(3, 1), 0.0);
}

TEST_P(NDManipTest, BlkdiagRectangularBlocks)
{
    // 2×3 block diag with 1×2 → 3×5
    eval("A = blkdiag([1 2 3; 4 5 6], [7 8]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 5u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 2), 6.0);
    EXPECT_DOUBLE_EQ((*A)(2, 3), 7.0);
    EXPECT_DOUBLE_EQ((*A)(2, 4), 8.0);
    // Top-right + bottom-left zero
    EXPECT_DOUBLE_EQ((*A)(0, 3), 0.0);
    EXPECT_DOUBLE_EQ((*A)(2, 0), 0.0);
}

TEST_P(NDManipTest, BlkdiagSingleArg)
{
    // blkdiag(A) = A
    eval("A = blkdiag([1 2; 3 4]);");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 4.0);
}

TEST_P(NDManipTest, BlkdiagThreeMatrices)
{
    eval("A = blkdiag([1], [2 3; 4 5], [6]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 4u);
    EXPECT_EQ(cols(*A), 4u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 2.0);
    EXPECT_DOUBLE_EQ((*A)(2, 2), 5.0);
    EXPECT_DOUBLE_EQ((*A)(3, 3), 6.0);
}

// ── permute ND (Phase 3a.3) ─────────────────────────────────────

TEST_P(NDManipTest, Permute4DRoundTrip)
{
    // 2×3×4×5 → permute [4 3 2 1] → 5×4×3×2. ipermute reverses.
    eval("A = reshape(1:120, [2, 3, 4, 5]); "
         "B = permute(A, [4, 3, 2, 1]); "
         "C = ipermute(B, [4, 3, 2, 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(A, C);"), 1.0);
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    const auto &dB = B->dims();
    EXPECT_EQ(dB.dim(0), 5u);
    EXPECT_EQ(dB.dim(1), 4u);
    EXPECT_EQ(dB.dim(2), 3u);
    EXPECT_EQ(dB.dim(3), 2u);
}

TEST_P(NDManipTest, Permute4DPreservesElementsViaLinear)
{
    // 4-arg subscript indexing isn't in the parser yet (Phase 6 follow-up).
    // Use linear indexing — flat-buffer access works at any rank.
    eval("A = reshape(1:120, [2, 3, 4, 5]); B = permute(A, [4, 3, 2, 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(120);"), 120.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1);"),   1.0);    // first elem stays first
    EXPECT_DOUBLE_EQ(evalScalar("B(120);"), 120.0);  // last stays last
    EXPECT_DOUBLE_EQ(evalScalar("sum(A(:)) == sum(B(:));"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(A) == numel(B);"), 1.0);
}

TEST_P(NDManipTest, Permute5DAllocatesAndIsInvertible)
{
    eval("A = reshape(1:720, [2, 3, 4, 5, 6]); "
         "B = permute(A, [5, 4, 3, 2, 1]); "
         "C = permute(B, [5, 4, 3, 2, 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(A, C);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ndims(B);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 1);"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 5);"), 2.0);
}

TEST_P(NDManipTest, Permute4DTransposeOfFirstTwoUsesFastPath)
{
    // [2 1 3 4] swaps the first two axes per "page" of the trailing
    // dims. Verified via shape + element-set preservation; positional
    // checks need 4-arg subscript indexing (Phase 6 follow-up).
    eval("A = reshape(1:120, [2, 3, 4, 5]); B = permute(A, [2, 1, 3, 4]);");
    auto *B = getVarPtr("B");
    const auto &dB = B->dims();
    EXPECT_EQ(dB.dim(0), 3u);
    EXPECT_EQ(dB.dim(1), 2u);
    EXPECT_EQ(dB.dim(2), 4u);
    EXPECT_EQ(dB.dim(3), 5u);
    EXPECT_DOUBLE_EQ(evalScalar("sum(A(:)) == sum(B(:));"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isequal(sort(A(:)), sort(B(:)));"), 1.0);
}

// ── squeeze ND (Phase 3a.2) ─────────────────────────────────────

TEST_P(NDManipTest, Squeeze4DTrailingSingleton)
{
    // 2×3×4×1 — a 2D-vector dim trailing 1 is already stripped by the
    // ones() constructor, so this collapses to 3D 2×3×4. squeeze is a
    // no-op past that since no internal singletons remain.
    eval("A = ones([2, 3, 4, 1]); B = squeeze(A); s = size(B);");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(cols(*s), 3u);
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[2], 4.0);
}

TEST_P(NDManipTest, Squeeze4DLeadingSingleton)
{
    // 1×3×4×5 → 3×4×5
    eval("A = ones([1, 3, 4, 5]); B = squeeze(A); s = size(B);");
    auto *s = getVarPtr("s");
    EXPECT_EQ(cols(*s), 3u);
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[1], 4.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[2], 5.0);
}

TEST_P(NDManipTest, Squeeze5DMixedSingletons)
{
    // 3×1×4×1×5 → 3×4×5
    eval("A = ones([3, 1, 4, 1, 5]); B = squeeze(A); s = size(B);");
    auto *s = getVarPtr("s");
    EXPECT_EQ(cols(*s), 3u);
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[1], 4.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[2], 5.0);
}

TEST_P(NDManipTest, SqueezeAllSingletonsCollapsesTo1x1)
{
    // 1×1×1×1 → 1×1 (preserve 2D minimum)
    eval("A = ones([1, 1, 1, 1]); B = squeeze(A); s = size(B);");
    auto *s = getVarPtr("s");
    EXPECT_EQ(cols(*s), 2u);
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[1], 1.0);
}

TEST_P(NDManipTest, SqueezeNDPreservesElementCount)
{
    // 2×1×3×1×4 → 2×3×4. Same element count.
    eval("A = ones([2, 1, 3, 1, 4]); B = squeeze(A);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B);"), 24.0);
    EXPECT_DOUBLE_EQ(evalScalar("ndims(B);"), 3.0);
}

INSTANTIATE_DUAL(NDManipTest);
