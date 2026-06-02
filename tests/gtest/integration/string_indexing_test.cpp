// tests/gtest/integration/string_indexing_test.cpp
//
// Paren-indexing of string arrays must match MATLAB and behave identically
// on both backends. The Bytecode VM already routed STRING through the
// generic indexGet/elemAt path; the TreeWalker's indexed-read dispatch
// omitted STRING from its type list, so `s(1)` threw "Cannot index into
// variable ... of type string". These regression tests pin the parity.

#include "dual_engine_fixture.hpp"

using namespace m_test;
using namespace numkit;

class StringIndexing : public DualEngineTest {};

TEST_P(StringIndexing, ScalarIndex)
{
    eval("clear; s = [\"a\" \"b\" \"c\"];");
    EXPECT_EQ(evalString("s(1)"), "a");
    EXPECT_EQ(evalString("s(2)"), "b");
    EXPECT_EQ(evalString("s(3)"), "c");
}

TEST_P(StringIndexing, EndIndex)
{
    eval("clear; s = [\"a\" \"b\" \"c\"];");
    EXPECT_EQ(evalString("s(end)"), "c");
}

TEST_P(StringIndexing, RangeIndex)
{
    eval("clear; s = [\"a\" \"b\" \"c\" \"d\"]; t = s(2:3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(t)"), 2.0);
    EXPECT_EQ(evalString("t(1)"), "b");
    EXPECT_EQ(evalString("t(2)"), "c");
}

TEST_P(StringIndexing, VectorIndex)
{
    eval("clear; s = [\"a\" \"b\" \"c\"]; t = s([1 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(t)"), 2.0);
    EXPECT_EQ(evalString("t(1)"), "a");
    EXPECT_EQ(evalString("t(2)"), "c");
}

TEST_P(StringIndexing, LogicalIndex)
{
    eval("clear; s = [\"a\" \"b\" \"c\"]; t = s([true false true]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(t)"), 2.0);
    EXPECT_EQ(evalString("t(1)"), "a");
    EXPECT_EQ(evalString("t(2)"), "c");
}

TEST_P(StringIndexing, TwoDIndex)
{
    // 2×2 string matrix; (row, col) indexing returns the element string.
    eval("clear; s = [\"a\" \"b\"; \"c\" \"d\"];");
    EXPECT_EQ(evalString("s(1,1)"), "a");
    EXPECT_EQ(evalString("s(2,1)"), "c");
    EXPECT_EQ(evalString("s(1,2)"), "b");
    EXPECT_EQ(evalString("s(2,2)"), "d");
}

TEST_P(StringIndexing, IndexExpressionResult)
{
    // Index the result of a non-variable expression (cell content) —
    // exercises the other TW dispatch site (target.isString()).
    eval("clear; c = {[\"x\" \"y\" \"z\"]};");
    EXPECT_EQ(evalString("c{1}(2)"), "y");
}

INSTANTIATE_DUAL(StringIndexing);
