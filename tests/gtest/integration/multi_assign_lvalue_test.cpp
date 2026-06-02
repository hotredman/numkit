// tests/gtest/integration/multi_assign_lvalue_test.cpp
//
// Multi-output assignment into complex lvalue targets:
//   [s.f] = deal(v)        — struct field
//   [a(i), b(j)] = ...     — indexed
//   [c{i}] = ...           — cell content
//   [s.(e)] = ...          — dynamic field
//   [d(i).f] = ...         — struct-array element field
//   [s.a.b] = ...          — nested field chain
//   mixed / ~ / real multi-output builtins (size, deal)
//
// Before this change the parser only accepted bare identifiers / ~ as
// multi-assign targets; any complex lvalue fell back to a matrix literal
// and produced "Invalid assignment target". Runs on both backends.

#include "dual_engine_fixture.hpp"
#include <algorithm>

using namespace m_test;
using namespace numkit;

class MultiAssignLValue : public DualEngineTest
{
protected:
    bool hasVarNamed(const std::string &name)
    {
        auto names = engine.workspaceVarNames();
        return std::find(names.begin(), names.end(), name) != names.end();
    }
};

TEST_P(MultiAssignLValue, SingleStructField)
{
    eval("clear; [s.f] = deal(3);");
    EXPECT_DOUBLE_EQ(evalScalar("s.f"), 3.0);
}

TEST_P(MultiAssignLValue, TwoStructFields)
{
    eval("clear; [s.a, s.b] = deal(1, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("s.a"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.b"), 2.0);
}

TEST_P(MultiAssignLValue, FieldsFromSizeBuiltin)
{
    // Real multi-output builtin feeding struct fields.
    eval("clear; [s.r, s.c] = size(zeros(3, 4));");
    EXPECT_DOUBLE_EQ(evalScalar("s.r"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.c"), 4.0);
}

TEST_P(MultiAssignLValue, QualifiedNamespaceMultiOutput)
{
    // [a,b] = pkg.fn(x): a qualified-namespace call feeding multiple
    // outputs. Previously compileMultiAssign took only the leaf name
    // ("findpeaks") and the call failed as undefined; now it routes the
    // full dotted name. findpeaks([0 2 0 5 0 3 0]) → peaks (2,5,3) at
    // locations (2,4,6).
    eval("clear; [pks, locs] = compat.findpeaks([0 2 0 5 0 3 0]);");
    EXPECT_EQ(eval("pks").numel(), 3u);
    EXPECT_DOUBLE_EQ(evalScalar("locs(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("locs(3)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("pks(2)"), 5.0);
}

TEST_P(MultiAssignLValue, IndexedTargets)
{
    eval("clear; a = zeros(1, 4); [a(2), a(4)] = deal(7, 9);");
    EXPECT_DOUBLE_EQ(evalScalar("a(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(4)"), 9.0);
}

TEST_P(MultiAssignLValue, IndexedAutoGrow)
{
    eval("clear; [a(3)] = deal(5);");
    EXPECT_DOUBLE_EQ(evalScalar("a(3)"), 5.0);
    EXPECT_EQ(eval("a").numel(), 3u);
}

TEST_P(MultiAssignLValue, CellContent)
{
    eval("clear; c = cell(1, 2); [c{1}, c{2}] = deal(10, 20);");
    EXPECT_DOUBLE_EQ(evalScalar("c{1}"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2}"), 20.0);
}

TEST_P(MultiAssignLValue, MixedPlainVarAndField)
{
    eval("clear; [x, s.f] = deal(11, 22);");
    EXPECT_DOUBLE_EQ(evalScalar("x"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.f"), 22.0);
}

TEST_P(MultiAssignLValue, TildeWithField)
{
    eval("clear; [~, s.f] = deal(1, 5);");
    // The discarded output must not leak into the workspace; only `s`
    // (and the field f) is created. Check BEFORE reading `s.f` as an
    // expression, since a bare `s.f` read would itself set `ans`.
    EXPECT_FALSE(hasVarNamed("ans"));
    EXPECT_FALSE(hasVarNamed("deal"));
    EXPECT_DOUBLE_EQ(evalScalar("s.f"), 5.0);
}

TEST_P(MultiAssignLValue, IndexedTargetWithVariableIndex)
{
    // Index expression reads a workspace variable from inside the lvalue
    // target — exercises the pre-import scan over lhsTargets (VM).
    eval("clear; a = zeros(1, 5); k = 3; [a(k)] = deal(42);");
    EXPECT_DOUBLE_EQ(evalScalar("a(3)"), 42.0);
}

TEST_P(MultiAssignLValue, DynamicField)
{
    eval("clear; nm = 'fld'; [s.(nm)] = deal(6);");
    EXPECT_DOUBLE_EQ(evalScalar("s.fld"), 6.0);
}

TEST_P(MultiAssignLValue, NestedFieldChain)
{
    eval("clear; [s.a.b] = deal(4);");
    EXPECT_DOUBLE_EQ(evalScalar("s.a.b"), 4.0);
}

TEST_P(MultiAssignLValue, StructArrayElementField)
{
    eval("clear; [d(2).x] = deal(8);");
    EXPECT_DOUBLE_EQ(evalScalar("d(2).x"), 8.0);
}

// Regression guard: plain identifier multi-assign must keep working
// (fast path untouched).
TEST_P(MultiAssignLValue, PlainIdentifiersStillWork)
{
    eval("clear; [p, q, r] = deal(1, 2, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("q"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("r"), 3.0);
}

// A bracketed expression that is NOT an assignment must still parse as a
// matrix literal (parser backtracking must not swallow it).
TEST_P(MultiAssignLValue, BracketExprNotMisparsedAsAssign)
{
    EXPECT_DOUBLE_EQ(evalScalar("clear; s.f = 2; v = [s.f, s.f]; sum(v)"), 4.0);
}

INSTANTIATE_DUAL(MultiAssignLValue);
