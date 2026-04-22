// tests/test_known_issues.cpp — Tests for known MATLAB incompatibilities
//
// These tests document bugs found during dual-backend test migration.
// Each test should PASS after the corresponding fix is applied.
//
// Run only these:
//   ./m_tests --gtest_filter="TW_VM/KnownIssue*"
//
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace m_test;

// ============================================================
// ISSUE #1: Short-circuit && / || in compound conditions
//
// MATLAB guarantees short-circuit evaluation for && and ||:
//   if j >= 1 && v(j) > key   — when j < 1, v(j) must NOT be evaluated
//
// Currently: both operands are evaluated, causing out-of-bounds
// ============================================================

class KnownIssueShortCircuit : public DualEngineTest {};

TEST_P(KnownIssueShortCircuit, AndSkipsRightOnFalse)
{
    // v(0) would throw out-of-bounds, but && must not evaluate it
    eval("v = [10 20 30];");
    eval("j = 0;");
    EXPECT_NO_THROW(eval("r = j >= 1 && v(j) > 5;"));
    EXPECT_DOUBLE_EQ(getVar("r"), 0.0);
}

TEST_P(KnownIssueShortCircuit, OrSkipsRightOnTrue)
{
    // v(0) would throw, but || must not evaluate it
    eval("v = [10 20 30];");
    eval("j = 0;");
    EXPECT_NO_THROW(eval("r = j < 1 || v(j) > 5;"));
    EXPECT_DOUBLE_EQ(getVar("r"), 1.0);
}

TEST_P(KnownIssueShortCircuit, WhileConditionShortCircuit)
{
    // Classic insertion-sort pattern: while j >= 1 && arr(j) > key
    eval(R"(
        v = [3 1 2];
        j = 0;
        r = 0;
        if j >= 1 && v(j) > 0
            r = 1;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("r"), 0.0);
}

TEST_P(KnownIssueShortCircuit, InsertionSortWithShortCircuit)
{
    // Full insertion sort using && in while — the original failing test
    eval(R"(
        function v = isort(v)
            n = length(v);
            for i = 2:n
                key = v(i);
                j = i - 1;
                while j >= 1 && v(j) > key
                    v(j+1) = v(j);
                    j = j - 1;
                end
                v(j+1) = key;
            end
        end
    )");
    eval("r = isort([4 2 5 1 3]);");
    auto *r = getVarPtr("r");
    ASSERT_EQ(r->numel(), 5u);
    for (size_t i = 0; i < 5; ++i)
        EXPECT_DOUBLE_EQ(r->doubleData()[i], static_cast<double>(i + 1));
}

INSTANTIATE_DUAL(KnownIssueShortCircuit);

// ============================================================
// ISSUE #2: Indexed assign on array inside function (VM)
//
// v(j+1) = v(j) inside a user function produces wrong results in VM.
// The VM doesn't properly handle indexed assignment on local arrays.
//
// TreeWalker works correctly.
// ============================================================

class KnownIssueIndexedAssign : public DualEngineTest {};

TEST_P(KnownIssueIndexedAssign, SimpleIndexedAssignInFunction)
{
    eval(R"(
        function out = f()
            out = [10 20 30];
            out(2) = 99;
        end
    )");
    eval("r = f();");
    auto *r = getVarPtr("r");
    ASSERT_EQ(r->numel(), 3u);
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 10.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[1], 99.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[2], 30.0);
}

TEST_P(KnownIssueIndexedAssign, SwapInFunction)
{
    eval(R"(
        function v = myswap(v)
            tmp = v(1);
            v(1) = v(2);
            v(2) = tmp;
        end
    )");
    eval("r = myswap([10 20]);");
    auto *r = getVarPtr("r");
    ASSERT_EQ(r->numel(), 2u);
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 20.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[1], 10.0);
}

TEST_P(KnownIssueIndexedAssign, ShiftElementsInFunction)
{
    // Shift elements right — the pattern that broke InsertionSort
    eval(R"(
        function out = shift_right(arr)
            out = arr;
            out(3) = out(2);
            out(2) = out(1);
            out(1) = 0;
        end
    )");
    eval("r = shift_right([10 20 30]);");
    auto *r = getVarPtr("r");
    ASSERT_EQ(r->numel(), 3u);
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[1], 10.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[2], 20.0);
}

INSTANTIATE_DUAL(KnownIssueIndexedAssign);

// ============================================================
// ISSUE #2b: Colon linear-assign `z(:) = rhs` in VM
//
// MATLAB: `z(:) = rhs` overwrites every element of an existing
// array `z` in linear order, preserving z's shape. Valid when
// rhs is a scalar (broadcast) or numel(rhs) == numel(z).
//
// Bug (pre-fix): VM's INDEX_SET was calling resolveIndicesUnchecked
// on the index, which for ':' returns an empty vector by contract
// ("caller must handle this separately"). INDEX_SET didn't handle
// it, so indexSet(indices=0, rhs.numel()=N) raised a spurious
// "left and right sides have a different number of elements"
// regardless of shape. TreeWalker was unaffected — its resolveIndex
// expands ':' to [0..numel-1] directly. See MVM.cpp INDEX_SET.
// ============================================================

class ColonLinearAssign : public DualEngineTest {};

TEST_P(ColonLinearAssign, ColumnVectorRhsMatchesShape)
{
    eval(R"(
        z = zeros(5, 1);
        x = [10; 20; 30; 40; 50];
        z(:) = x;
    )");
    auto *z = getVarPtr("z");
    ASSERT_EQ(z->numel(), 5u);
    EXPECT_EQ(rows(*z), 5u);
    EXPECT_EQ(cols(*z), 1u);
    expectElem(*z, 0, 10.0);
    expectElem(*z, 1, 20.0);
    expectElem(*z, 2, 30.0);
    expectElem(*z, 3, 40.0);
    expectElem(*z, 4, 50.0);
}

TEST_P(ColonLinearAssign, RowVectorRhsFlattenedIntoColumn)
{
    // MATLAB: z(:) = rhs keeps z's shape. A row-vector rhs with
    // same numel is accepted and written in linear (column-major)
    // order into z.
    eval(R"(
        z = zeros(3, 1);
        z(:) = [7 8 9];
    )");
    auto *z = getVarPtr("z");
    ASSERT_EQ(rows(*z), 3u);
    ASSERT_EQ(cols(*z), 1u);
    expectElem(*z, 0, 7.0);
    expectElem(*z, 1, 8.0);
    expectElem(*z, 2, 9.0);
}

TEST_P(ColonLinearAssign, ScalarRhsBroadcasts)
{
    eval(R"(
        z = zeros(4, 1);
        z(:) = 3.5;
    )");
    auto *z = getVarPtr("z");
    ASSERT_EQ(z->numel(), 4u);
    for (size_t i = 0; i < 4; ++i)
        expectElem(*z, i, 3.5);
}

TEST_P(ColonLinearAssign, PreservesShapeOfTwoDMatrix)
{
    // z is 2x3 — `z(:) = rhs` must not reshape z; it writes six
    // elements in column-major order while keeping z 2x3.
    eval(R"(
        z = zeros(2, 3);
        z(:) = [1 2 3 4 5 6];
    )");
    auto *z = getVarPtr("z");
    ASSERT_EQ(rows(*z), 2u);
    ASSERT_EQ(cols(*z), 3u);
    // Column-major traversal: (0,0), (1,0), (0,1), (1,1), (0,2), (1,2)
    expectElem2D(*z, 0, 0, 1.0);
    expectElem2D(*z, 1, 0, 2.0);
    expectElem2D(*z, 0, 1, 3.0);
    expectElem2D(*z, 1, 1, 4.0);
    expectElem2D(*z, 0, 2, 5.0);
    expectElem2D(*z, 1, 2, 6.0);
}

TEST_P(ColonLinearAssign, MismatchedSizeThrows)
{
    eval("z = zeros(5, 1);");
    EXPECT_THROW(eval("z(:) = [1 2 3];"), std::exception);
}

TEST_P(ColonLinearAssign, RepeatedAssignReusesSlot)
{
    // The benchmark pattern that motivated this fix: loop over a
    // pre-allocated output buffer. Just a smoke-test that the loop
    // runs without throwing and leaves z in the expected final state.
    eval(R"(
        z = zeros(4, 1);
        for k = 1:3
            z(:) = k + [10; 20; 30; 40];
        end
    )");
    auto *z = getVarPtr("z");
    ASSERT_EQ(z->numel(), 4u);
    expectElem(*z, 0, 13.0);
    expectElem(*z, 1, 23.0);
    expectElem(*z, 2, 33.0);
    expectElem(*z, 3, 43.0);
}

TEST_P(ColonLinearAssign, ComplexRhsIntoComplexLhs)
{
    // Both sides complex, matching numel — exercises the complex
    // memcpy fast path in VM INDEX_SET.
    eval(R"(
        z = complex(zeros(3, 1), zeros(3, 1));
        z(:) = [1+2i; 3+4i; 5+6i];
    )");
    auto *z = getVarPtr("z");
    ASSERT_TRUE(z->isComplex());
    ASSERT_EQ(z->numel(), 3u);
    EXPECT_DOUBLE_EQ(z->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(z->complexData()[0].imag(), 2.0);
    EXPECT_DOUBLE_EQ(z->complexData()[1].real(), 3.0);
    EXPECT_DOUBLE_EQ(z->complexData()[1].imag(), 4.0);
    EXPECT_DOUBLE_EQ(z->complexData()[2].real(), 5.0);
    EXPECT_DOUBLE_EQ(z->complexData()[2].imag(), 6.0);
}

TEST_P(ColonLinearAssign, ComplexRhsPromotesDoubleLhs)
{
    // z starts real; assigning complex values into z(:) must
    // promote z to complex via the generic fallback path.
    eval(R"(
        z = zeros(2, 1);
        z(:) = [1+2i; 3+4i];
    )");
    auto *z = getVarPtr("z");
    ASSERT_TRUE(z->isComplex());
    ASSERT_EQ(z->numel(), 2u);
    EXPECT_DOUBLE_EQ(z->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(z->complexData()[0].imag(), 2.0);
    EXPECT_DOUBLE_EQ(z->complexData()[1].real(), 3.0);
    EXPECT_DOUBLE_EQ(z->complexData()[1].imag(), 4.0);
}

INSTANTIATE_DUAL(ColonLinearAssign);

// ============================================================
// ISSUE #2c: Grow-by-one indexed assign `A(end+1) = x`
//
// Classic MATLAB pattern for building a vector incrementally.
// Before the fix, the VM path called ensureSize + elemSet every
// iteration and ensureSize allocated *exactly* numel+1 elements,
// giving O(N²) work for an N-length build. After the fix, when
// `i == numel(A)` the VM routes through MValue::appendScalar which
// preserves a geometric capacity — amortised O(1) per append.
//
// Tests below validate semantics, not timing. The benchmark that
// motivated the fix lives in docs/examples/Benchmark/.
// ============================================================

class GrowByOneAssign : public DualEngineTest {};

TEST_P(GrowByOneAssign, FromEmptyViaIndexOne)
{
    // The starting point of the classic build loop.
    eval(R"(
        A = [];
        A(1) = 42;
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->numel(), 1u);
    ASSERT_EQ(rows(*A), 1u);
    ASSERT_EQ(cols(*A), 1u);
    expectElem(*A, 0, 42.0);
}

TEST_P(GrowByOneAssign, IncrementalBuildFromEmpty)
{
    // Ten grow-by-ones starting from empty. Each step hits the
    // appendScalar fast path once A is a heap row-vector.
    eval(R"(
        A = [];
        for i = 1:10
            A(i) = i * 2;
        end
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->numel(), 10u);
    ASSERT_EQ(rows(*A), 1u);
    ASSERT_EQ(cols(*A), 10u);
    for (size_t i = 0; i < 10; ++i)
        expectElem(*A, i, static_cast<double>(2 * (i + 1)));
}

TEST_P(GrowByOneAssign, EndPlusOneSyntax)
{
    // `end+1` is the explicit MATLAB idiom; must give the same
    // observable result as `A(i) = x` with i = numel+1.
    eval(R"(
        A = [];
        for i = 1:5
            A(end+1) = i * 10;
        end
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->numel(), 5u);
    ASSERT_EQ(rows(*A), 1u);
    for (size_t i = 0; i < 5; ++i)
        expectElem(*A, i, static_cast<double>(10 * (i + 1)));
}

TEST_P(GrowByOneAssign, MixedGrowAndRewrite)
{
    // Growing, then rewriting already-written slots must keep the
    // final array correct. Rewrites do NOT go through appendScalar.
    eval(R"(
        A = [];
        A(1) = 10;
        A(2) = 20;
        A(3) = 30;
        A(2) = 99;
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->numel(), 3u);
    expectElem(*A, 0, 10.0);
    expectElem(*A, 1, 99.0);
    expectElem(*A, 2, 30.0);
}

TEST_P(GrowByOneAssign, GrowByMoreThanOneFallback)
{
    // When i skips ahead of numel+1 the fast path declines; the
    // generic ensureSize + elemSet path must still zero-fill the
    // intervening slots.
    eval(R"(
        A = [];
        A(1) = 10;
        A(5) = 50;
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->numel(), 5u);
    expectElem(*A, 0, 10.0);
    expectElem(*A, 1, 0.0);
    expectElem(*A, 2, 0.0);
    expectElem(*A, 3, 0.0);
    expectElem(*A, 4, 50.0);
}

TEST_P(GrowByOneAssign, ColumnVectorGrowth)
{
    // Column-vector start: A(end+1) appends a new row. The fast
    // path is row-vector-only so this goes through the generic
    // ensureSize path — the result must still be a column.
    eval(R"(
        A = [1; 2; 3];
        A(4) = 4;
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->numel(), 4u);
    ASSERT_EQ(rows(*A), 4u);
    ASSERT_EQ(cols(*A), 1u);
    for (size_t i = 0; i < 4; ++i)
        expectElem(*A, i, static_cast<double>(i + 1));
}

TEST_P(GrowByOneAssign, ComplexRhsNotFastPath)
{
    // Complex rhs should route around the double-only fast path
    // and still promote A to complex correctly.
    eval(R"(
        A = [];
        A(1) = 1 + 2i;
        A(2) = 3 + 4i;
    )");
    auto *A = getVarPtr("A");
    ASSERT_TRUE(A->isComplex());
    ASSERT_EQ(A->numel(), 2u);
    EXPECT_DOUBLE_EQ(A->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(A->complexData()[0].imag(), 2.0);
    EXPECT_DOUBLE_EQ(A->complexData()[1].real(), 3.0);
    EXPECT_DOUBLE_EQ(A->complexData()[1].imag(), 4.0);
}

TEST_P(GrowByOneAssign, LargeBuildRemainsCorrect)
{
    // Stress the capacity-doubling path: build a 1000-element
    // vector and check every value. The purpose is correctness,
    // not speed — a broken capacity tracker tends to corrupt the
    // last element of every geometric step.
    eval(R"(
        A = [];
        for i = 1:1000
            A(i) = i;
        end
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->numel(), 1000u);
    for (size_t i = 0; i < 1000; ++i)
        expectElem(*A, i, static_cast<double>(i + 1));
}

INSTANTIATE_DUAL(GrowByOneAssign);

// ============================================================
// ISSUE #2d: HORZCAT-grow rewrite `A = [A, x]`
//
// The MATLAB-canonical incremental-build idiom. The compiler
// detects this exact AST shape (LHS identifier, RHS a one-row
// matrix literal whose first element is the same identifier and
// whose second element is any expression) and emits the
// HORZCAT_APPEND opcode instead of staging two registers and
// calling MValue::horzcat. At runtime HORZCAT_APPEND uses the
// appendScalar fast path when conditions allow (empty/row-vector
// heap-double dst with unique ownership and a real-scalar rhs)
// and otherwise falls back to a normal two-element horzcat — so
// the rewrite must preserve the observable semantics across all
// shape combinations.
// ============================================================

class HorzcatGrowRewrite : public DualEngineTest {};

TEST_P(HorzcatGrowRewrite, RowVectorAppendOne)
{
    eval(R"(
        A = [];
        A = [A, 1];
        A = [A, 2];
        A = [A, 3];
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->numel(), 3u);
    ASSERT_EQ(rows(*A), 1u);
    ASSERT_EQ(cols(*A), 3u);
    expectElem(*A, 0, 1.0);
    expectElem(*A, 1, 2.0);
    expectElem(*A, 2, 3.0);
}

TEST_P(HorzcatGrowRewrite, IncrementalBuildLoop)
{
    // The benchmark_interp.m test 5 pattern in miniature.
    eval(R"(
        A = [];
        for i = 1:10
            A = [A, i * 2];
        end
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->numel(), 10u);
    ASSERT_EQ(rows(*A), 1u);
    for (size_t i = 0; i < 10; ++i)
        expectElem(*A, i, static_cast<double>(2 * (i + 1)));
}

TEST_P(HorzcatGrowRewrite, ColumnVectorAppendFallsBackToHorzcat)
{
    // A is 3x1 (column). The fast path in HORZCAT_APPEND must NOT
    // misinterpret this as "append one element to a row vector"
    // and call appendScalar — that would yield a 1x4 instead of
    // numkit-m's broadcast-to-column behaviour. Confirm the
    // fallback path runs and produces a 3x2 (MATLAB raises
    // dim-mismatch here; numkit-m chooses to broadcast — that is
    // a separate question not in scope for this rewrite).
    eval(R"(
        A = [1; 2; 3];
        A = [A, 4];
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(rows(*A), 3u);
    ASSERT_EQ(cols(*A), 2u);
    expectElem2D(*A, 0, 0, 1.0);
    expectElem2D(*A, 1, 0, 2.0);
    expectElem2D(*A, 2, 0, 3.0);
    expectElem2D(*A, 0, 1, 4.0);
}

TEST_P(HorzcatGrowRewrite, NonScalarRhsFallsBack)
{
    // Two-element row vector concat: [A, B] where B is a row
    // vector. The fast path declines (rhs not scalar) and the
    // fallback horzcat runs.
    eval(R"(
        A = [1, 2];
        B = [3, 4];
        A = [A, B];
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->numel(), 4u);
    ASSERT_EQ(rows(*A), 1u);
    expectElem(*A, 0, 1.0);
    expectElem(*A, 1, 2.0);
    expectElem(*A, 2, 3.0);
    expectElem(*A, 3, 4.0);
}

TEST_P(HorzcatGrowRewrite, ComplexScalarRhsPromotes)
{
    // Real row-vector A, complex scalar rhs. Fast path declines
    // (val.isComplex()), fallback horzcat promotes A to complex
    // and produces a complex row vector.
    eval(R"(
        A = [1, 2, 3];
        A = [A, 4 + 5i];
    )");
    auto *A = getVarPtr("A");
    ASSERT_TRUE(A->isComplex());
    ASSERT_EQ(A->numel(), 4u);
    EXPECT_DOUBLE_EQ(A->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(A->complexData()[0].imag(), 0.0);
    EXPECT_DOUBLE_EQ(A->complexData()[3].real(), 4.0);
    EXPECT_DOUBLE_EQ(A->complexData()[3].imag(), 5.0);
}

TEST_P(HorzcatGrowRewrite, ExpressionRhs)
{
    // The second element of the row literal is a non-trivial
    // expression. Compiler must compile it correctly and pass
    // its register to HORZCAT_APPEND.
    eval(R"(
        A = [];
        A = [A, 2 * 3 + 1];
        A = [A, sin(0)];
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->numel(), 2u);
    expectElem(*A, 0, 7.0);
    expectElem(*A, 1, 0.0);
}

TEST_P(HorzcatGrowRewrite, NotPatternMatched_DifferentIdentifier)
{
    // RHS first element is a DIFFERENT identifier, not the LHS.
    // Pattern doesn't match — must compile via the standard
    // matrix-literal path. Sanity check that we didn't over-match.
    eval(R"(
        A = [1, 2, 3];
        B = [4, 5];
        C = [A, B];
    )");
    auto *C = getVarPtr("C");
    ASSERT_EQ(C->numel(), 5u);
    expectElem(*C, 0, 1.0);
    expectElem(*C, 4, 5.0);
}

TEST_P(HorzcatGrowRewrite, NotPatternMatched_ThreeElements)
{
    // RHS has three elements rather than two. Pattern requires
    // exactly two — falls through to the standard HORZCAT path.
    eval(R"(
        A = [1, 2];
        A = [A, 3, 4];
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->numel(), 4u);
    expectElem(*A, 0, 1.0);
    expectElem(*A, 1, 2.0);
    expectElem(*A, 2, 3.0);
    expectElem(*A, 3, 4.0);
}

TEST_P(HorzcatGrowRewrite, TwoDMatrixAppendColumn)
{
    // A = [A, col] where A is 2x2 and col is 2x1.
    // Fast path declines (rhs not scalar), fallback horzcat
    // produces a 2x3 matrix.
    eval(R"(
        A = [1 2; 3 4];
        A = [A, [5; 6]];
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(rows(*A), 2u);
    ASSERT_EQ(cols(*A), 3u);
    expectElem2D(*A, 0, 0, 1.0);
    expectElem2D(*A, 0, 1, 2.0);
    expectElem2D(*A, 0, 2, 5.0);
    expectElem2D(*A, 1, 0, 3.0);
    expectElem2D(*A, 1, 1, 4.0);
    expectElem2D(*A, 1, 2, 6.0);
}

TEST_P(HorzcatGrowRewrite, TwoDMatrixAppendMatrix)
{
    // A = [A, B] where both are 2x2 → 2x4.
    eval(R"(
        A = [1 2; 3 4];
        A = [A, [10 20; 30 40]];
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(rows(*A), 2u);
    ASSERT_EQ(cols(*A), 4u);
    expectElem2D(*A, 0, 0, 1.0);
    expectElem2D(*A, 0, 1, 2.0);
    expectElem2D(*A, 0, 2, 10.0);
    expectElem2D(*A, 0, 3, 20.0);
    expectElem2D(*A, 1, 0, 3.0);
    expectElem2D(*A, 1, 1, 4.0);
    expectElem2D(*A, 1, 2, 30.0);
    expectElem2D(*A, 1, 3, 40.0);
}

TEST_P(HorzcatGrowRewrite, ThreeDArrayHorzcat)
{
    // A = [A, B] where both are 2x2x3 → 2x4x3 along columns.
    // Stresses the fallback horzcat's 3D branch.
    eval(R"(
        A = ones(2, 2, 3);
        B = ones(2, 2, 3) * 7;
        A = [A, B];
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->dims().rows(), 2u);
    ASSERT_EQ(A->dims().cols(), 4u);
    ASSERT_EQ(A->dims().pages(), 3u);
    // Spot-check a few elements: first half of each page is 1, second half is 7.
    EXPECT_DOUBLE_EQ(A->doubleData()[A->dims().sub2ind(0, 0, 0)], 1.0);
    EXPECT_DOUBLE_EQ(A->doubleData()[A->dims().sub2ind(1, 1, 0)], 1.0);
    EXPECT_DOUBLE_EQ(A->doubleData()[A->dims().sub2ind(0, 2, 0)], 7.0);
    EXPECT_DOUBLE_EQ(A->doubleData()[A->dims().sub2ind(1, 3, 2)], 7.0);
    EXPECT_DOUBLE_EQ(A->doubleData()[A->dims().sub2ind(0, 0, 2)], 1.0);
    EXPECT_DOUBLE_EQ(A->doubleData()[A->dims().sub2ind(1, 3, 1)], 7.0);
}

TEST_P(HorzcatGrowRewrite, LargeBuildRemainsCorrect)
{
    // Stress the HORZCAT_APPEND fast path: 1000 grow iterations.
    eval(R"(
        A = [];
        for i = 1:1000
            A = [A, i];
        end
    )");
    auto *A = getVarPtr("A");
    ASSERT_EQ(A->numel(), 1000u);
    ASSERT_EQ(rows(*A), 1u);
    for (size_t i = 0; i < 1000; ++i)
        expectElem(*A, i, static_cast<double>(i + 1));
}

INSTANTIATE_DUAL(HorzcatGrowRewrite);

// ============================================================
// ISSUE #3: nargin not supported in VM
//
// nargin should return the number of arguments passed to
// the current function. Works in TreeWalker, fails in VM.
// ============================================================

class KnownIssueNargin : public DualEngineTest {};

TEST_P(KnownIssueNargin, NarginZero)
{
    eval(R"(
        function y = f()
            y = nargin;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("f();"), 0.0);
}

TEST_P(KnownIssueNargin, NarginOne)
{
    eval(R"(
        function y = f(a)
            y = nargin;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("f(42);"), 1.0);
}

TEST_P(KnownIssueNargin, NarginTwo)
{
    eval(R"(
        function y = f(a, b)
            y = nargin;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("f(1, 2);"), 2.0);
}

TEST_P(KnownIssueNargin, NarginOptionalArg)
{
    eval(R"(
        function y = add(a, b)
            if nargin < 2
                b = 10;
            end
            y = a + b;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("add(5);"), 15.0);
    EXPECT_DOUBLE_EQ(evalScalar("add(5, 3);"), 8.0);
}

INSTANTIATE_DUAL(KnownIssueNargin);

// ============================================================
// ISSUE #4: islogical() missing or broken in VM
//
// islogical(x) should return true for logical values.
// Currently not registered as externalFunc or VM can't compile it.
// ============================================================

class KnownIssueIsLogical : public DualEngineTest {};

TEST_P(KnownIssueIsLogical, IsLogicalTrue)
{
    EXPECT_TRUE(evalBool("islogical(true);"));
}

TEST_P(KnownIssueIsLogical, IsLogicalFalse)
{
    EXPECT_TRUE(evalBool("islogical(false);"));
}

TEST_P(KnownIssueIsLogical, IsLogicalOnDouble)
{
    EXPECT_FALSE(evalBool("islogical(42);"));
}

TEST_P(KnownIssueIsLogical, IsLogicalOnArray)
{
    eval("r = islogical([1 0 1] > 0);");
    // comparison result should be logical
}

INSTANTIATE_DUAL(KnownIssueIsLogical);

// ============================================================
// ISSUE #5: Comparison operators return double instead of logical
//
// In MATLAB: 3 > 2 returns logical(1), not double(1)
// This affects islogical(), element-wise &/|, and logical indexing type.
// ============================================================

class KnownIssueComparisonType : public DualEngineTest {};

TEST_P(KnownIssueComparisonType, GreaterThanReturnsLogical)
{
    eval("r = 3 > 2;");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_TRUE(r->isLogical()) << "3 > 2 should return logical, not double";
}

TEST_P(KnownIssueComparisonType, LessThanReturnsLogical)
{
    eval("r = 1 < 5;");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_TRUE(r->isLogical()) << "1 < 5 should return logical, not double";
}

TEST_P(KnownIssueComparisonType, EqualReturnsLogical)
{
    eval("r = 3 == 3;");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_TRUE(r->isLogical()) << "3 == 3 should return logical, not double";
}

TEST_P(KnownIssueComparisonType, NotEqualReturnsLogical)
{
    eval("r = 3 ~= 4;");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_TRUE(r->isLogical()) << "3 ~= 4 should return logical, not double";
}

TEST_P(KnownIssueComparisonType, ArrayComparisonReturnsLogical)
{
    eval("r = [1 2 3] > 2;");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_TRUE(r->isLogical()) << "[1 2 3] > 2 should return logical array";
    ASSERT_EQ(r->numel(), 3u);
    const uint8_t *d = r->logicalData();
    EXPECT_EQ(d[0], 0);
    EXPECT_EQ(d[1], 0);
    EXPECT_EQ(d[2], 1);
}

INSTANTIATE_DUAL(KnownIssueComparisonType);

// ============================================================
// ISSUE #6: Empty string '' not supported
//
// In MATLAB: s = '' creates a 1x0 char array.
// Currently fails to parse or evaluate on both backends.
// ============================================================

class KnownIssueEmptyString : public DualEngineTest {};

TEST_P(KnownIssueEmptyString, EmptyStringLiteral)
{
    eval("s = '';");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->toString(), "");
}

TEST_P(KnownIssueEmptyString, EmptyStringLength)
{
    eval("s = ''; n = length(s);");
    EXPECT_DOUBLE_EQ(getVar("n"), 0.0);
}

TEST_P(KnownIssueEmptyString, EmptyStringConcat)
{
    eval("s = ['' 'hello'];");
    EXPECT_EQ(getVarPtr("s")->toString(), "hello");
}

TEST_P(KnownIssueEmptyString, StrcmpWithEmpty)
{
    EXPECT_TRUE(evalBool("strcmp('', '');"));
    EXPECT_FALSE(evalBool("strcmp('', 'a');"));
}

INSTANTIATE_DUAL(KnownIssueEmptyString);