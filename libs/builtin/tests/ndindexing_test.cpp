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

TEST_P(NDIndexingTest, Write4DAutoGrowsOnOOB)
{
    // Polish round-2 item 6: ND scalar assign past current dims now
    // grows the target instead of throwing (matches MATLAB).
    eval("A = zeros([2, 3, 4, 5]); A(3, 1, 1, 1) = 5;");
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 2);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(3, 1, 1, 1);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 1, 1);"), 0.0);

    eval("B = zeros([2, 3, 4, 5]); B(1, 1, 1, 6) = 7;");
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 4);"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 6);"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 3, 4, 5);"), 0.0);
}

TEST_P(NDIndexingTest, Write5DCreatesFromUninitialized)
{
    // Auto-create + auto-grow at rank 5 from an empty/unset variable.
    eval("clear C; C(2, 1, 3, 1, 2) = 42;");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(C);"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 3);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 5);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2, 1, 3, 1, 2);"), 42.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1, 1, 1, 1, 1);"), 0.0);
}

TEST_P(NDIndexingTest, Write4DGrowsRankFrom2D)
{
    // 2D → 4D auto-grow when the assignment uses 4 indices and the
    // higher dims exceed 1.
    eval("D = zeros(2, 3); D(2, 3, 2, 2) = 11;");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(D);"),   4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(D, 3);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(D, 4);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(2, 3, 2, 2);"), 11.0);
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

// ── ND format / disp (Phase 3d) ─────────────────────────────────

TEST_P(NDIndexingTest, Disp4DEmitsHeadersForEachOuterPage)
{
    // 2x2x2x2 has 4 outer pages: (1,1), (2,1), (1,2), (2,2). disp must
    // emit a header for each. capturedOutput collects engine stdout.
    capturedOutput.clear();
    eval("disp(reshape(1:16, [2, 2, 2, 2]));");
    EXPECT_NE(capturedOutput.find("(:,:,1,1)"), std::string::npos);
    EXPECT_NE(capturedOutput.find("(:,:,2,1)"), std::string::npos);
    EXPECT_NE(capturedOutput.find("(:,:,1,2)"), std::string::npos);
    EXPECT_NE(capturedOutput.find("(:,:,2,2)"), std::string::npos);
}

TEST_P(NDIndexingTest, Disp5DEmitsFiveCommaHeaders)
{
    capturedOutput.clear();
    eval("disp(reshape(1:8, [2, 2, 1, 1, 2]));");
    // Two pages along axis 5; axes 3..4 are singletons.
    EXPECT_NE(capturedOutput.find("(:,:,1,1,1)"), std::string::npos);
    EXPECT_NE(capturedOutput.find("(:,:,1,1,2)"), std::string::npos);
}

// ── 6D smoke + ND-empty input coverage (Phase 4) ────────────────

TEST_P(NDIndexingTest, SixDSmokeAllOpsBasicSanity)
{
    // 2×2×2×2×2×2 = 64 elements. Exercise all the ops we lifted to ND.
    eval("A = reshape(1:64, [2, 2, 2, 2, 2, 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(A);"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(A);"), 64.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 6);"), 2.0);
    // Subscript read at all corners
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 1, 1, 1, 1);"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 2, 2, 2, 2, 2);"), 64.0);
    // Sum along last axis
    EXPECT_DOUBLE_EQ(evalScalar("s = sum(A, 6); s(1, 1, 1, 1, 1, 1);"),
                     1.0 + 33.0);  // A(1,1,1,1,1,1)=1 + A(1,1,1,1,1,2)=33
    EXPECT_DOUBLE_EQ(evalScalar("size(s, 6);"), 1.0);
    // Permute axes 1..6 reversed
    EXPECT_DOUBLE_EQ(evalScalar("B = permute(A, [6 5 4 3 2 1]);"
                                "B(1, 1, 1, 1, 1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 2, 2, 2, 2, 2);"), 64.0);
    // Elementwise scalar ×
    EXPECT_DOUBLE_EQ(evalScalar("C = A * 2; C(2, 2, 2, 2, 2, 2);"), 128.0);
    EXPECT_DOUBLE_EQ(evalScalar("ndims(C);"), 6.0);
}

TEST_P(NDIndexingTest, NDZeroDimShape)
{
    // A 4D with one zero dim. zeros() should accept the zero-dim shape
    // and return a tensor with the expected per-axis sizes (numel = 0).
    eval("A = zeros([2, 0, 3, 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(A);"),   4.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(A);"),   0.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 2);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 4);"), 2.0);
}

// ── Phase A audit: end-keyword 5D, 2-arg subscript-write 4D, ND broadcast row×col

TEST_P(NDIndexingTest, EndKeyword5D)
{
    eval("A = reshape(1:720, [2, 3, 4, 5, 6]);");
    EXPECT_DOUBLE_EQ(evalScalar("A(end, end, end, end, end);"), 720.0);
    // A(1,1,1,1,end) — last along axis 5 = 6. lin0 = 0+0+0+0+5*120=600 → 601
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 1, 1, end);"), 601.0);
    // A(end, end, 1, 1, 1) — A(2,3,1,1,1) lin0 = 1+2*2 = 5 → 6
    EXPECT_DOUBLE_EQ(evalScalar("A(end, end, 1, 1, 1);"), 6.0);
}

TEST_P(NDIndexingTest, TwoArgSubscriptWriteOn4DInput)
{
    // A is 4D. A(i, j) = scalar — what does MATLAB do? It assigns to a 1D
    // linear interpretation: A(i,j) treats remaining dims as flat. We follow
    // the same convention: 2-arg write targets the first 2 dims if other
    // dims are 1, otherwise should throw or treat as linear. Expected
    // behaviour here: should NOT silently corrupt the 4D buffer.
    eval("A = reshape(1:24, [2, 3, 2, 2]);");
    // Either it should reject (MATLAB error) or write correctly. We don't
    // care which path — just that it doesn't return garbage.
    bool threwOrSucceeded = false;
    try {
        eval("A(1, 1) = 99;");
        // If it succeeded, A(1,1,1,1) should be 99 (overwrite first elem).
        threwOrSucceeded = (evalScalar("A(1, 1, 1, 1);") == 99.0);
    } catch (...) {
        threwOrSucceeded = true;  // accepted: throws is OK
    }
    EXPECT_TRUE(threwOrSucceeded);
}

TEST_P(NDIndexingTest, NDBroadcastRowAndColumnOn4D)
{
    // 1×3×1×2 row-tile + 2×1×4×1 col-tile → 2×3×4×2 result.
    eval("r = reshape([10 20 30 40 50 60], [1, 3, 1, 2]);"
         "c = reshape([1 2], [2, 1, 1, 1]);"
         "A = r + c;");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(A);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 2);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 3);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 4);"), 2.0);
    // A(1, 1, 1, 1) = r(1,1,1,1) + c(1,1,1,1) = 10 + 1 = 11
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 1, 1);"), 11.0);
    // A(2, 3, 1, 2) = r(1,3,1,2) + c(2,1,1,1) = 60 + 2 = 62
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 3, 1, 2);"), 62.0);
}

// ── CELL ND indexing (Polish round-2 item 5) ───────────────────────
//
// Build a 2×2×2×2 cell, write per-element MValues, then read scalar +
// slice + delete a slab. Exercises indexGetND/indexSetND/indexDeleteND
// CELL paths and the cellND factory.

TEST_P(NDIndexingTest, Cell4DScalarReadWrite)
{
    eval("C = cell(2, 2, 2, 2);"
         "C{1, 1, 1, 1} = 'first';"
         "C{2, 2, 2, 2} = 'last';"
         "C{1, 2, 1, 2} = 42;");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(C);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(C);"), 16.0);
    // Scalar reads
    EXPECT_DOUBLE_EQ(evalScalar("C{1, 2, 1, 2};"), 42.0);
    // String returns CHAR vector — check via length
    EXPECT_DOUBLE_EQ(evalScalar("numel(C{1, 1, 1, 1});"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(C{2, 2, 2, 2});"), 4.0);
}

TEST_P(NDIndexingTest, Cell4DSliceCopy)
{
    // Build C(2,2,1,1)..(2,2,2,2), assign one slab, then slice it.
    eval("C = cell(2, 2, 1, 2);"
         "C{1, 1, 1, 1} = 11;"
         "C{2, 1, 1, 1} = 21;"
         "C{1, 2, 1, 1} = 12;"
         "C{2, 2, 1, 1} = 22;"
         "C{1, 1, 1, 2} = 110;"
         "C{2, 2, 1, 2} = 220;"
         "S = C(:, :, 1, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(S);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("S{1, 1};"),   110.0);
    EXPECT_DOUBLE_EQ(evalScalar("S{2, 2};"),   220.0);
}

TEST_P(NDIndexingTest, Cell4DDeleteSlab)
{
    // Delete one slab along axis 4 — result shrinks to 2×2×1×1.
    eval("C = cell(2, 2, 1, 2);"
         "C{1, 1, 1, 1} = 'a';"
         "C{1, 1, 1, 2} = 'b';"
         "C(:, :, :, 2) = [];");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(C);"),   4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 4);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(C);"),   4.0);
    // Surviving element should be 'a'
    EXPECT_DOUBLE_EQ(evalScalar("numel(C{1, 1, 1, 1});"), 1.0);
}

TEST_P(NDIndexingTest, Cell4DBraceAutoGrowsOnOOB)
{
    // Round-3 Phase 1: brace-cell ND assign now grows the target instead
    // of throwing. Matches MATLAB `C{i,j,k,l} = v`.
    eval("C = cell(2, 2, 2, 2); C{3, 4, 1, 1} = 'x';");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(C);"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 2);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 3);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 4);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(C);"), 48.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(C{3, 4, 1, 1});"), 1.0);
}

TEST_P(NDIndexingTest, Cell5DBraceCreatesFromUninitialized)
{
    // Auto-create + auto-grow at rank 5 from an undefined variable.
    eval("clear D; D{2, 1, 3, 1, 2} = 42;");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(D);"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(D, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(D, 3);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(D, 5);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("D{2, 1, 3, 1, 2};"), 42.0);
}

TEST_P(NDIndexingTest, Cell4DBraceGrowsRankFrom2D)
{
    // 2D cell → 4D auto-grow when assignment uses 4 indices and the
    // higher dims exceed 1.
    eval("E = cell(2, 2); E{2, 2, 1, 4} = 'last';");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(E);"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(E, 3);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(E, 4);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(E{2, 2, 1, 4});"), 4.0);
}

TEST_P(NDIndexingTest, Cell4DBracePreservesOldDataOnGrow)
{
    // Existing cell content must survive the growth.
    eval("F = cell(2, 2, 1, 2); F{1, 1, 1, 1} = 'a'; F{2, 2, 1, 2} = 'z';"
         "F{3, 3, 2, 3} = 'new';");
    EXPECT_DOUBLE_EQ(evalScalar("size(F, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(F, 2);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(F, 3);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(F, 4);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(F{1, 1, 1, 1});"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(F{2, 2, 1, 2});"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(F{3, 3, 2, 3});"), 3.0);
}

TEST_P(NDIndexingTest, Numeric4DDeleteRowsAcrossSlab)
{
    // Smoke for non-CELL ND delete: drop row 1 from a 4D numeric tensor.
    eval("A = reshape(1:24, [2, 3, 2, 2]); A(1, :, :, :) = [];");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(A);"),   4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 2);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(A);"),   12.0);
    // Original A(2,1,1,1)=2 → now A(1,1,1,1)=2
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1, 1, 1);"), 2.0);
    // Original A(2,3,2,2)=24 → now A(1,3,2,2)=24
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 3, 2, 2);"), 24.0);
}

INSTANTIATE_DUAL(NDIndexingTest);
