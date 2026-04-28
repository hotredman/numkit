// libs/builtin/tests/cellfun_test.cpp
//
// cellfun + structfun. Built-in handles only — custom anonymous
// handles intentionally throw m:cellfun:fnUnsupported until the
// engine callback API lands (round 11 item 27).

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class CellFunTest : public DualEngineTest
{};

// ── cellfun: shape predicates ──────────────────────────────────

TEST_P(CellFunTest, CellfunNumel)
{
    eval("c = {1, [2 3], [4;5;6], []};"
         "n = cellfun(@numel, c);");
    auto *n = getVarPtr("n");
    EXPECT_EQ(rows(*n), 1u);
    EXPECT_EQ(cols(*n), 4u);
    EXPECT_DOUBLE_EQ(n->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(n->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(n->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(n->doubleData()[3], 0.0);
}

TEST_P(CellFunTest, CellfunLength)
{
    eval("c = {[1 2 3 4], [5;6], 7};"
         "n = cellfun(@length, c);");
    auto *n = getVarPtr("n");
    EXPECT_DOUBLE_EQ(n->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(n->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(n->doubleData()[2], 1.0);
}

TEST_P(CellFunTest, CellfunIsempty)
{
    eval("c = {[], [1], [], 'x'};"
         "n = cellfun(@isempty, c);");
    auto *n = getVarPtr("n");
    EXPECT_EQ(n->type(), MType::LOGICAL);
    EXPECT_NE(n->logicalData()[0], 0);
    EXPECT_EQ(n->logicalData()[1], 0);
    EXPECT_NE(n->logicalData()[2], 0);
    EXPECT_EQ(n->logicalData()[3], 0);
}

TEST_P(CellFunTest, CellfunPreservesShape)
{
    // 2x3 cell array → 2x3 numeric output.
    eval("c = {1 2 3; 4 5 6};"
         "n = cellfun(@numel, c);");
    auto *n = getVarPtr("n");
    EXPECT_EQ(rows(*n), 2u);
    EXPECT_EQ(cols(*n), 3u);
    for (size_t i = 0; i < 6; ++i)
        EXPECT_DOUBLE_EQ(n->doubleData()[i], 1.0);
}

// ── cellfun: type predicates ───────────────────────────────────

TEST_P(CellFunTest, CellfunIsnumeric)
{
    eval("c = {1, 'x', true, [1 2 3]};"
         "n = cellfun(@isnumeric, c);");
    auto *n = getVarPtr("n");
    EXPECT_EQ(n->type(), MType::LOGICAL);
    EXPECT_NE(n->logicalData()[0], 0);
    EXPECT_EQ(n->logicalData()[1], 0);
    EXPECT_EQ(n->logicalData()[2], 0);
    EXPECT_NE(n->logicalData()[3], 0);
}

TEST_P(CellFunTest, CellfunIschar)
{
    eval("c = {1, 'hi', [1 2]};"
         "n = cellfun(@ischar, c);");
    auto *n = getVarPtr("n");
    EXPECT_EQ(n->logicalData()[0], 0);
    EXPECT_NE(n->logicalData()[1], 0);
    EXPECT_EQ(n->logicalData()[2], 0);
}

TEST_P(CellFunTest, CellfunIslogical)
{
    eval("c = {true, 1, false};"
         "n = cellfun(@islogical, c);");
    auto *n = getVarPtr("n");
    EXPECT_NE(n->logicalData()[0], 0);
    EXPECT_EQ(n->logicalData()[1], 0);
    EXPECT_NE(n->logicalData()[2], 0);
}

TEST_P(CellFunTest, CellfunIsreal)
{
    eval("c = {1, 1+2i, 0+0i};"
         "n = cellfun(@isreal, c);");
    auto *n = getVarPtr("n");
    EXPECT_NE(n->logicalData()[0], 0);
    EXPECT_EQ(n->logicalData()[1], 0);
    // 0+0i is still typed COMPLEX → isreal = false.
    EXPECT_EQ(n->logicalData()[2], 0);
}

// ── cellfun: numeric reductions ────────────────────────────────

TEST_P(CellFunTest, CellfunSum)
{
    eval("c = {[1 2 3], [10 20 30 40], [5]};"
         "n = cellfun(@sum, c);");
    auto *n = getVarPtr("n");
    EXPECT_DOUBLE_EQ(n->doubleData()[0], 6.0);
    EXPECT_DOUBLE_EQ(n->doubleData()[1], 100.0);
    EXPECT_DOUBLE_EQ(n->doubleData()[2], 5.0);
}

TEST_P(CellFunTest, CellfunMean)
{
    eval("c = {[2 4 6], [10 20]};"
         "n = cellfun(@mean, c);");
    auto *n = getVarPtr("n");
    EXPECT_DOUBLE_EQ(n->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(n->doubleData()[1], 15.0);
}

TEST_P(CellFunTest, CellfunProd)
{
    eval("c = {[2 3 4], [10 5]};"
         "n = cellfun(@prod, c);");
    auto *n = getVarPtr("n");
    EXPECT_DOUBLE_EQ(n->doubleData()[0], 24.0);
    EXPECT_DOUBLE_EQ(n->doubleData()[1], 50.0);
}

// ── cellfun: scalar-required predicates throw on non-scalar in uniform mode

TEST_P(CellFunTest, CellfunIsnanScalarOnly)
{
    eval("c = {NaN, 1, NaN};"
         "n = cellfun(@isnan, c);");
    auto *n = getVarPtr("n");
    EXPECT_NE(n->logicalData()[0], 0);
    EXPECT_EQ(n->logicalData()[1], 0);
    EXPECT_NE(n->logicalData()[2], 0);
}

TEST_P(CellFunTest, CellfunIsnanThrowsForNonScalar)
{
    eval("c = {NaN, [1 NaN], 3};");
    EXPECT_THROW(eval("n = cellfun(@isnan, c);"), std::exception);
}

// ── cellfun: UniformOutput false ───────────────────────────────

TEST_P(CellFunTest, CellfunUniformFalseGivesCellOutput)
{
    eval("c = {[1 2 3], 'hi', true};"
         "n = cellfun(@class, c, 'UniformOutput', false);");
    auto *n = getVarPtr("n");
    EXPECT_TRUE(n->isCell());
    EXPECT_EQ(n->numel(), 3u);
    EXPECT_EQ(n->cellAt(0).toString(), "double");
    EXPECT_EQ(n->cellAt(1).toString(), "char");
    EXPECT_EQ(n->cellAt(2).toString(), "logical");
}

TEST_P(CellFunTest, CellfunClassUniformTrueThrows)
{
    // @class returns char string — can't pack into uniform numeric/logical.
    eval("c = {1, 'x'};");
    EXPECT_THROW(eval("n = cellfun(@class, c);"), std::exception);
}

TEST_P(CellFunTest, CellfunUniformFalseSumPreservesNonScalarOutputs)
{
    // sum on row vector returns scalar — pack into cell anyway.
    eval("c = {[1 2 3], [10 20]};"
         "n = cellfun(@sum, c, 'UniformOutput', false);");
    auto *n = getVarPtr("n");
    EXPECT_TRUE(n->isCell());
    EXPECT_EQ(n->numel(), 2u);
    EXPECT_DOUBLE_EQ(n->cellAt(0).toScalar(), 6.0);
    EXPECT_DOUBLE_EQ(n->cellAt(1).toScalar(), 30.0);
}

// ── cellfun: error paths ───────────────────────────────────────

TEST_P(CellFunTest, CellfunNonCellSecondArgThrows)
{
    EXPECT_THROW(eval("n = cellfun(@numel, [1 2 3]);"), std::exception);
}

TEST_P(CellFunTest, CellfunUnsupportedHandleThrows)
{
    eval("c = {1, 2};");
    EXPECT_THROW(eval("n = cellfun(@(x) x+1, c);"), std::exception);
}

TEST_P(CellFunTest, CellfunUnknownFlagThrows)
{
    eval("c = {1, 2};");
    EXPECT_THROW(eval("n = cellfun(@numel, c, 'NoSuchFlag', true);"), std::exception);
}

TEST_P(CellFunTest, CellfunFlagWithoutValueThrows)
{
    eval("c = {1, 2};");
    EXPECT_THROW(eval("n = cellfun(@numel, c, 'UniformOutput');"), std::exception);
}

TEST_P(CellFunTest, CellfunEmptyCellGivesEmptyOutput)
{
    eval("c = cell(0, 0);"
         "n = cellfun(@numel, c);");
    auto *n = getVarPtr("n");
    EXPECT_EQ(n->numel(), 0u);
    EXPECT_EQ(rows(*n), 0u);
    EXPECT_EQ(cols(*n), 0u);
}

// ── structfun ──────────────────────────────────────────────────

TEST_P(CellFunTest, StructfunNumel)
{
    eval("s.a = [1 2 3];"
         "s.b = [4;5];"
         "s.c = 7;"
         "v = structfun(@numel, s);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 1u);
    // Field iteration order is alphabetical (std::map): a, b, c.
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 1.0);
}

TEST_P(CellFunTest, StructfunIsempty)
{
    eval("s.x = [];"
         "s.y = 1;"
         "v = structfun(@isempty, s);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->type(), MType::LOGICAL);
    EXPECT_NE(v->logicalData()[0], 0);
    EXPECT_EQ(v->logicalData()[1], 0);
}

TEST_P(CellFunTest, StructfunSum)
{
    eval("s.a = [1 2 3];"
         "s.b = [10 20];"
         "v = structfun(@sum, s);");
    auto *v = getVarPtr("v");
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 6.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 30.0);
}

TEST_P(CellFunTest, StructfunUniformFalseClass)
{
    eval("s.a = 1;"
         "s.b = 'hello';"
         "v = structfun(@class, s, 'UniformOutput', false);");
    auto *v = getVarPtr("v");
    EXPECT_TRUE(v->isCell());
    EXPECT_EQ(v->numel(), 2u);
    EXPECT_EQ(v->cellAt(0).toString(), "double");
    EXPECT_EQ(v->cellAt(1).toString(), "char");
}

TEST_P(CellFunTest, StructfunNonStructThrows)
{
    EXPECT_THROW(eval("v = structfun(@numel, [1 2 3]);"), std::exception);
}

TEST_P(CellFunTest, StructfunEmptyStruct)
{
    eval("s = struct();"
         "v = structfun(@numel, s);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 0u);
    EXPECT_EQ(rows(*v), 0u);
    EXPECT_EQ(cols(*v), 1u);
}

INSTANTIATE_DUAL(CellFunTest);
