// tests/test_builtins.cpp — Built-in functions (math, array creation, strings)
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

class BuiltinTest : public DualEngineTest {};

// ── Array creation ──────────────────────────────────────────

TEST_P(BuiltinTest, Zeros)
{
    eval("A = zeros(2, 3);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 3u);
    for (size_t i = 0; i < A->numel(); ++i)
        EXPECT_DOUBLE_EQ(A->doubleData()[i], 0.0);
}

TEST_P(BuiltinTest, Ones)
{
    eval("A = ones(2, 2);");
    auto *A = getVarPtr("A");
    for (size_t i = 0; i < A->numel(); ++i)
        EXPECT_DOUBLE_EQ(A->doubleData()[i], 1.0);
}

TEST_P(BuiltinTest, Eye)
{
    eval("I = eye(3);");
    auto *I = getVarPtr("I");
    EXPECT_DOUBLE_EQ((*I)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*I)(1, 1), 1.0);
    EXPECT_DOUBLE_EQ((*I)(0, 1), 0.0);
}

// ── Size / length / numel ───────────────────────────────────

TEST_P(BuiltinTest, Size)
{
    eval("A = ones(3, 5);");
    eval("r = size(A, 1);");
    EXPECT_DOUBLE_EQ(getVar("r"), 3.0);
    eval("c = size(A, 2);");
    EXPECT_DOUBLE_EQ(getVar("c"), 5.0);
}

TEST_P(BuiltinTest, Length)
{
    eval("v = [1 2 3 4 5]; l = length(v);");
    EXPECT_DOUBLE_EQ(getVar("l"), 5.0);
}

TEST_P(BuiltinTest, Numel)
{
    eval("A = ones(3, 4); n = numel(A);");
    EXPECT_DOUBLE_EQ(getVar("n"), 12.0);
}

// ── Aggregation ─────────────────────────────────────────────

TEST_P(BuiltinTest, Sum)
{
    eval("r = sum([1 2 3 4 5]);");
    EXPECT_DOUBLE_EQ(getVar("r"), 15.0);
}

TEST_P(BuiltinTest, MinMax)
{
    eval("function [a, b] = mymin(v)\n  a = min(v);\n  b = 0;\n  for i = 1:length(v)\n    if v(i) "
         "== a, b = i; break; end\n  end\nend");
    eval("[mn, mi] = mymin([3 1 4 1 5]);");
    EXPECT_DOUBLE_EQ(getVar("mn"), 1.0);
    EXPECT_DOUBLE_EQ(getVar("mi"), 2.0);
}

// ── Math functions ──────────────────────────────────────────

TEST_P(BuiltinTest, MathFunctions)
{
    EXPECT_NEAR(evalScalar("sin(pi/2);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("cos(0);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sqrt(16);"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("abs(-5);"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("exp(0);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("log(1);"), 0.0, 1e-12);
}

TEST_P(BuiltinTest, Floor)
{
    EXPECT_DOUBLE_EQ(evalScalar("floor(3.7);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("floor(-3.2);"), -4.0);
}

TEST_P(BuiltinTest, Mod)
{
    EXPECT_DOUBLE_EQ(evalScalar("mod(7, 3);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("mod(10, 5);"), 0.0);
}

// ── Reshape ─────────────────────────────────────────────────

TEST_P(BuiltinTest, Reshape)
{
    eval("A = [1 2 3 4 5 6]; B = reshape(A, 2, 3);");
    auto *B = getVarPtr("B");
    EXPECT_EQ(rows(*B), 2u);
    EXPECT_EQ(cols(*B), 3u);
}

// ── Error ───────────────────────────────────────────────────

TEST_P(BuiltinTest, ErrorFunction)
{
    EXPECT_THROW(eval("error('test error');"), std::runtime_error);
}

// ── String functions ────────────────────────────────────────

TEST_P(BuiltinTest, Strcmp)
{
    EXPECT_TRUE(evalBool("strcmp('hello', 'hello');"));
    EXPECT_FALSE(evalBool("strcmp('hello', 'world');"));
}

// ── Logspace ────────────────────────────────────────────────

TEST_P(BuiltinTest, BasicLogspace)
{
    eval("x = logspace(0, 3, 4);");
    auto *x = getVarPtr("x");
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(x->numel(), 4u);
    EXPECT_NEAR(x->doubleData()[0], 1.0, 1e-10);
    EXPECT_NEAR(x->doubleData()[1], 10.0, 1e-10);
    EXPECT_NEAR(x->doubleData()[2], 100.0, 1e-10);
    EXPECT_NEAR(x->doubleData()[3], 1000.0, 1e-10);
}

TEST_P(BuiltinTest, LogspaceTwoPoints)
{
    eval("x = logspace(1, 2, 2);");
    auto *x = getVarPtr("x");
    EXPECT_NEAR(x->doubleData()[0], 10.0, 1e-10);
    EXPECT_NEAR(x->doubleData()[1], 100.0, 1e-10);
}

TEST_P(BuiltinTest, LogspaceDefaultN)
{
    eval("x = logspace(0, 1);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->numel(), 50u);
    EXPECT_NEAR(x->doubleData()[0], 1.0, 1e-10);
    EXPECT_NEAR(x->doubleData()[49], 10.0, 1e-10);
}

TEST_P(BuiltinTest, LogspaceSinglePoint)
{
    eval("x = logspace(2, 2, 1);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->numel(), 1u);
    EXPECT_NEAR(x->doubleData()[0], 100.0, 1e-10);
}

INSTANTIATE_DUAL(BuiltinTest);

// ============================================================
// Display / output
// ============================================================

class DisplayTest : public DualEngineTest {};

TEST_P(DisplayTest, SuppressOutput)
{
    capturedOutput.clear();
    eval("42;");
    EXPECT_TRUE(capturedOutput.empty());
}

TEST_P(DisplayTest, ShowOutput)
{
    capturedOutput.clear();
    eval("42");
    EXPECT_FALSE(capturedOutput.empty());
    EXPECT_NE(capturedOutput.find("42"), std::string::npos);
}

INSTANTIATE_DUAL(DisplayTest);
