// tests/gtest/integration/compound_lvalue_test.cpp
//
// Assignment into compound / nested lvalue targets that mix accessor
// kinds — struct fields, paren indexing, struct-array elements and cell
// content — to arbitrary depth. Previously these either threw
// ("Invalid indexed assignment target" on the TreeWalker) or silently
// corrupted state (the VM wrote a phantom variable for `s.x(2)=…`).
//
// Both the single-target (`s.x(2)=v`) and the multi-output
// (`[s.x(2), c{1}.f] = deal(...)`) forms are exercised on both engines.

#include "dual_engine_fixture.hpp"
#include <algorithm>

using namespace m_test;
using namespace numkit;

class CompoundLValue : public DualEngineTest
{
protected:
    bool hasVarNamed(const std::string &name)
    {
        auto names = engine.workspaceVarNames();
        return std::find(names.begin(), names.end(), name) != names.end();
    }
};

// ── field-then-index ─────────────────────────────────────────
TEST_P(CompoundLValue, FieldThenIndex)
{
    eval("clear; s.x = [0 0 0]; s.x(2) = 5;");
    EXPECT_DOUBLE_EQ(evalScalar("s.x(2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.x(1)"), 0.0);
    // Must NOT have leaked a phantom variable named after the field.
    EXPECT_FALSE(hasVarNamed("x"));
}

TEST_P(CompoundLValue, FieldThenIndexAutoGrow)
{
    eval("clear; s.x(3) = 7;");        // s.x created empty then grown
    EXPECT_DOUBLE_EQ(evalScalar("s.x(3)"), 7.0);
    EXPECT_EQ(eval("s.x").numel(), 3u);
}

TEST_P(CompoundLValue, FieldThen2DIndex)
{
    eval("clear; s.m = zeros(2,2); s.m(2,2) = 9;");
    EXPECT_DOUBLE_EQ(evalScalar("s.m(2,2)"), 9.0);
}

TEST_P(CompoundLValue, FieldThenIndexEnd)
{
    eval("clear; s.x = [1 2 3]; s.x(end) = 30;");
    EXPECT_DOUBLE_EQ(evalScalar("s.x(3)"), 30.0);
}

// ── nested field chains ──────────────────────────────────────
TEST_P(CompoundLValue, NestedField)
{
    eval("clear; s.a.b = 7;");
    EXPECT_DOUBLE_EQ(evalScalar("s.a.b"), 7.0);
}

// ── struct-array element, deep ───────────────────────────────
TEST_P(CompoundLValue, StructArrayElemNestedField)
{
    eval("clear; d(2).a.b = 7;");
    EXPECT_DOUBLE_EQ(evalScalar("d(2).a.b"), 7.0);
}

TEST_P(CompoundLValue, StructArrayElemFieldIndex)
{
    eval("clear; d(2).a = [0 0]; d(2).a(2) = 9;");
    EXPECT_DOUBLE_EQ(evalScalar("d(2).a(2)"), 9.0);
}

TEST_P(CompoundLValue, StructArrayElemAutoGrow)
{
    eval("clear; d(3).v = 5;");
    EXPECT_DOUBLE_EQ(evalScalar("d(3).v"), 5.0);
}

// ── cell content as a container ──────────────────────────────
TEST_P(CompoundLValue, CellContentIndex)
{
    eval("clear; c = {[1 2 3]}; c{1}(2) = 8;");
    EXPECT_DOUBLE_EQ(evalScalar("c{1}(2)"), 8.0);
}

TEST_P(CompoundLValue, CellContentField)
{
    eval("clear; c = {struct()}; c{1}.f = 4;");
    EXPECT_DOUBLE_EQ(evalScalar("c{1}.f"), 4.0);
}

TEST_P(CompoundLValue, CellContentFieldAutoCreate)
{
    eval("clear; c{2}.f = 6;");        // c grown, content coerced to struct
    EXPECT_DOUBLE_EQ(evalScalar("c{2}.f"), 6.0);
}

// ── deep mix: field → struct-array elem → field ──────────────
TEST_P(CompoundLValue, DeepMixWriteRead)
{
    // field → struct-array element → field, written and read back on both
    // engines (the rvalue read of a paren-indexed struct-array field is
    // exercised here too).
    eval("clear; a.b(2).c = 3;");
    EXPECT_DOUBLE_EQ(evalScalar("a.b(2).c"), 3.0);
}

// ── dynamic field as a container ─────────────────────────────
TEST_P(CompoundLValue, DynamicFieldThenIndex)
{
    eval("clear; nm = 'x'; s.(nm) = [0 0]; s.(nm)(2) = 11;");
    EXPECT_DOUBLE_EQ(evalScalar("s.x(2)"), 11.0);
}

// ── N-D containers (no artificial 2-D cap) ───────────────────
TEST_P(CompoundLValue, StructArray2DElemField)
{
    eval("clear; d(1,2).a = 5;");
    EXPECT_DOUBLE_EQ(evalScalar("d(1,2).a"), 5.0);
    EXPECT_EQ(rows(eval("d")), 1u);
    EXPECT_EQ(cols(eval("d")), 2u);
}

TEST_P(CompoundLValue, StructArray2DElemNestedField)
{
    eval("clear; d(2,3).a.b = 5;");
    EXPECT_DOUBLE_EQ(evalScalar("d(2,3).a.b"), 5.0);
}

TEST_P(CompoundLValue, StructArray3DElemField)
{
    eval("clear; d(1,1,2).v = 9;");
    EXPECT_DOUBLE_EQ(evalScalar("d(1,1,2).v"), 9.0);
}

TEST_P(CompoundLValue, Cell2DIntermediateField)
{
    eval("clear; c = cell(1,2); c{1,2}.f = 5;");
    EXPECT_DOUBLE_EQ(evalScalar("c{1,2}.f"), 5.0);
}

TEST_P(CompoundLValue, Cell2DIntermediateAutoGrow)
{
    eval("clear; c{2,3}.f = 7;");
    EXPECT_DOUBLE_EQ(evalScalar("c{2,3}.f"), 7.0);
}

// ── multi-output assignment into compound targets ────────────
TEST_P(CompoundLValue, MultiAssignCompoundTargets)
{
    eval("clear; s.x = [0 0]; c = {0}; [s.x(2), c{1}] = deal(5, 9);");
    EXPECT_DOUBLE_EQ(evalScalar("s.x(2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{1}"), 9.0);
}

TEST_P(CompoundLValue, MultiAssignStructArrayElems)
{
    eval("clear; [d(1).a, d(2).a] = deal(10, 20);");
    EXPECT_DOUBLE_EQ(evalScalar("d(1).a"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(2).a"), 20.0);
}

// ── regression: simple single-level lvalues still work ───────
TEST_P(CompoundLValue, SimpleIndexUnaffected)
{
    eval("clear; a = zeros(1,3); a(2) = 4;");
    EXPECT_DOUBLE_EQ(evalScalar("a(2)"), 4.0);
}

TEST_P(CompoundLValue, SimpleFieldUnaffected)
{
    eval("clear; s.f = 3;");
    EXPECT_DOUBLE_EQ(evalScalar("s.f"), 3.0);
}

INSTANTIATE_DUAL(CompoundLValue);
