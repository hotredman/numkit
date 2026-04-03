// tests/test_known_issues.cpp — Tests for known MATLAB incompatibilities
//
// These tests document bugs found during dual-backend test migration.
// Each test should PASS after the corresponding fix is applied.
//
// Run only these:
//   ./mlab_tests --gtest_filter="TW_VM/KnownIssue*"
//
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

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
    eval("r = isort([4 2 7 1 3]);");
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