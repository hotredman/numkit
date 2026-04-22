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

INSTANTIATE_DUAL(NDManipTest);
