// libs/builtin/tests/ndindexing_test.cpp — Phase 6: parser/interp ND indexing
//
// Subscript indexing with 4+ indices: read, write, slice, end-keyword,
// colon-all, OOB errors. Verifies both the TreeWalker (execIndexAccess /
// execIndexedAssign) and VM (CALL_INDIRECT and INDEX_GET_ND/INDEX_SET_ND)
// dispatch paths.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class NDIndexingTest : public DualEngineTest
{};

// ── 4D read ─────────────────────────────────────────────────────

TEST_P(NDIndexingTest, Read4DScalarFirstAndLast)
{
    // 2×3×4×5, A(:) = 1..120 column-major. Verify boundary positions
    // map to expected linear values.
    eval("A = reshape(1:120, [2, 3, 4, 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 1, 1);"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 1, 1, 1);"),   2.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 2, 1, 1);"),   3.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 3, 4, 5);"), 120.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 2, 3, 4);"),  87.0);
}

TEST_P(NDIndexingTest, Read4DColonSlicePerDim)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]);");
    // A(:, 1, 1, 1) → column [1; 2]
    EXPECT_DOUBLE_EQ(evalScalar("v = A(:, 1, 1, 1); numel(v);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2);"), 2.0);

    // A(2, :, 1, 1) is selected over dim 1 — result has shape preserved by
    // MATLAB's rules; just check element values via subscript.
    EXPECT_DOUBLE_EQ(evalScalar("w = A(2, :, 1, 1); numel(w);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(2);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(3);"), 6.0);
}

TEST_P(NDIndexingTest, Read4DRangeSlice)
{
    // A(1:2, 2:3, 1, 1) — 2×2 slice at fixed page+vol (2,2 inner shape)
    eval("A = reshape(1:120, [2, 3, 4, 5]); S = A(1:2, 2:3, 1, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(S);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("S(1, 1);"), 3.0); // A(1,2,1,1) = 3
    EXPECT_DOUBLE_EQ(evalScalar("S(2, 1);"), 4.0); // A(2,2,1,1) = 4
    EXPECT_DOUBLE_EQ(evalScalar("S(1, 2);"), 5.0); // A(1,3,1,1) = 5
    EXPECT_DOUBLE_EQ(evalScalar("S(2, 2);"), 6.0); // A(2,3,1,1) = 6
}

TEST_P(NDIndexingTest, Read4DEndKeyword)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("A(end, end, end, end);"), 120.0);
    // A(1,1,1,5): strides [1,2,6,24], lin0 = 4*24 = 96 → value 97
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 1, end);"), 97.0);
    // A(2,3,1,1): lin0 = 1 + 2*2 = 5 → value 6
    EXPECT_DOUBLE_EQ(evalScalar("A(end, end, 1, 1);"), 6.0);
}

TEST_P(NDIndexingTest, Read5DScalarBoundary)
{
    eval("A = reshape(1:720, [2, 3, 4, 5, 6]);");
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 1, 1, 1);"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 3, 4, 5, 6);"), 720.0);
    // A(2, 1, 1, 1, 2): (1) + 0 + 0 + 0 + 1*120 = 121 → 122
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 1, 1, 1, 2);"), 122.0);
}

TEST_P(NDIndexingTest, Read4DOOBThrows)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]);");
    EXPECT_THROW(eval("v = A(3, 1, 1, 1);"), std::exception);
    EXPECT_THROW(eval("v = A(1, 1, 1, 6);"), std::exception);
}

// ── 4D write ────────────────────────────────────────────────────

TEST_P(NDIndexingTest, Write4DScalar)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]); A(2, 3, 4, 5) = 999;");
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 3, 4, 5);"), 999.0);
    // Total still 120 elements, shape unchanged
    EXPECT_DOUBLE_EQ(evalScalar("numel(A);"), 120.0);
    EXPECT_DOUBLE_EQ(evalScalar("ndims(A);"),   4.0);
    // Other elements untouched
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 2, 3, 4);"), 87.0);
}

TEST_P(NDIndexingTest, Write4DScalarBroadcast)
{
    // Broadcast scalar 0 over a 2D slice
    eval("A = reshape(1:120, [2, 3, 4, 5]); A(:, :, 4, 5) = 0;");
    // The full 2×3 slice at page 4, vol 5 is now zero.
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 4, 5);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 3, 4, 5);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 2, 4, 5);"), 0.0);
    // Adjacent slice untouched. A(1,1,3,5) = 0+0+2*6+4*24+1 = 109
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 3, 5);"), 109.0);
}

TEST_P(NDIndexingTest, Write4DVectorRoundTrip)
{
    // Write a 2-vector down a column-of-1, then read it back.
    eval("A = zeros([2, 3, 4, 5]); A(:, 2, 3, 4) = [10; 20];");
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 2, 3, 4);"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 2, 3, 4);"), 20.0);
    // Adjacent slice unchanged
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 3, 4);"), 0.0);
}

TEST_P(NDIndexingTest, Write5DScalar)
{
    eval("A = zeros([2, 3, 4, 5, 6]); A(2, 3, 4, 5, 6) = 7;");
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 3, 4, 5, 6);"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 1, 1, 1);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("ndims(A);"), 5.0);
}

TEST_P(NDIndexingTest, Write4DOOBThrows)
{
    eval("A = zeros([2, 3, 4, 5]);");
    EXPECT_THROW(eval("A(3, 1, 1, 1) = 5;"), std::exception);
    EXPECT_THROW(eval("A(1, 1, 1, 6) = 5;"), std::exception);
}

// ── Round-trip via permute (uses 4-arg subscript both for verify and for the
//   Permute4D positional-element check we couldn't do in Phase 3a.3) ──────

TEST_P(NDIndexingTest, Permute4DPositionalVerification)
{
    // [4 3 2 1] reverses the axis order. After permute, B(d, c, b, a) = A(a, b, c, d).
    // Pick a few unambiguous cells and verify by direct subscript.
    eval("A = reshape(1:120, [2, 3, 4, 5]); B = permute(A, [4, 3, 2, 1]);");
    // A(1, 1, 1, 1) = 1   →   B(1, 1, 1, 1) = 1
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 1);"), 1.0);
    // A(2, 3, 4, 5) = 120 →   B(5, 4, 3, 2) = 120
    EXPECT_DOUBLE_EQ(evalScalar("B(5, 4, 3, 2);"), 120.0);
    // A(1, 2, 3, 4) = 87  →   B(4, 3, 2, 1) = 87
    EXPECT_DOUBLE_EQ(evalScalar("B(4, 3, 2, 1);"), 87.0);
    // A(2, 1, 1, 1) = 2   →   B(1, 1, 1, 2) = 2
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 2);"), 2.0);
}

TEST_P(NDIndexingTest, Permute4DSwapFirstTwoPositional)
{
    // [2 1 3 4] swaps the first two axes. B(j, i, k, l) = A(i, j, k, l).
    eval("A = reshape(1:120, [2, 3, 4, 5]); B = permute(A, [2, 1, 3, 4]);");
    // A(1, 2, 1, 1) = 3 → B(2, 1, 1, 1) = 3
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 1, 1, 1);"), 3.0);
    // A(2, 3, 4, 5) = 120 → B(3, 2, 4, 5) = 120
    EXPECT_DOUBLE_EQ(evalScalar("B(3, 2, 4, 5);"), 120.0);
}

INSTANTIATE_DUAL(NDIndexingTest);
