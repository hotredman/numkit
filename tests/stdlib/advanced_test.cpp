// tests/test_advanced.cpp — Advanced features: scope, closures, string ops,
//   nargin/nargout, comparison, type checks, edge cases
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace m_test;

// ============================================================
// Scope and closures
// ============================================================

class ScopeTest : public DualEngineTest
{};

TEST_P(ScopeTest, FunctionLocalScope)
{
    eval(R"(
        function y = f(x)
            local_var = 100;
            y = x + local_var;
        end
    )");
    eval("r = f(5);");
    EXPECT_DOUBLE_EQ(getVar("r"), 105.0);
    // local_var should not leak to global scope
    EXPECT_EQ(getVarPtr("local_var"), nullptr);
}

TEST_P(ScopeTest, NestedFunctionScope)
{
    eval(R"(
        function y = outer(x)
            function z = inner(a)
                z = a * 2;
            end
            y = inner(x) + 1;
        end
    )");
    eval("r = outer(5);");
    EXPECT_DOUBLE_EQ(getVar("r"), 11.0);
}

TEST_P(ScopeTest, ClosureCapturesByValue)
{
    eval(R"(
        a = 10;
        f = @(x) x + a;
        a = 999;
        r = f(5);
    )");
    // Closure captured a=10, not a=999
    EXPECT_DOUBLE_EQ(getVar("r"), 15.0);
}

TEST_P(ScopeTest, ClosureOverMultipleVars)
{
    eval(R"(
        a = 2; b = 3;
        f = @(x) a*x + b;
        r = f(5);
    )");
    EXPECT_DOUBLE_EQ(getVar("r"), 13.0);
}

TEST_P(ScopeTest, FunctionDoesNotModifyCallerVars)
{
    eval(R"(
        function y = f(x)
            x = x * 100;
            y = x;
        end
    )");
    eval("x = 5; r = f(x);");
    EXPECT_DOUBLE_EQ(getVar("x"), 5.0); // unchanged
    EXPECT_DOUBLE_EQ(getVar("r"), 500.0);
}

INSTANTIATE_DUAL(ScopeTest);

// ============================================================
// String operations
// ============================================================

class StringTest : public DualEngineTest
{};

TEST_P(StringTest, StringLiteral)
{
    eval("s = 'hello';");
    EXPECT_EQ(getVarPtr("s")->toString(), "hello");
}

TEST_P(StringTest, StringConcat)
{
    eval("s = ['hello' ' ' 'world'];");
    EXPECT_EQ(getVarPtr("s")->toString(), "hello world");
}

TEST_P(StringTest, Strcmp)
{
    EXPECT_TRUE(evalBool("strcmp('abc', 'abc');"));
    EXPECT_FALSE(evalBool("strcmp('abc', 'def');"));
}

TEST_P(StringTest, Num2str)
{
    eval("s = num2str(42);");
    EXPECT_EQ(getVarPtr("s")->toString(), "42");
}

TEST_P(StringTest, Sprintf)
{
    eval("s = sprintf('x = %d', 42);");
    EXPECT_EQ(getVarPtr("s")->toString(), "x = 42");
}

TEST_P(StringTest, StringLength)
{
    eval("s = 'hello'; n = length(s);");
    EXPECT_DOUBLE_EQ(getVar("n"), 5.0);
}

INSTANTIATE_DUAL(StringTest);

// ============================================================
// nargin / nargout
// ============================================================

class NarginTest : public DualEngineTest
{};

TEST_P(NarginTest, NarginDefault)
{
    eval(R"(
        function y = f(a, b)
            if nargin < 2
                b = 10;
            end
            y = a + b;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("f(5);"), 15.0);
    EXPECT_DOUBLE_EQ(evalScalar("f(5, 3);"), 8.0);
}

INSTANTIATE_DUAL(NarginTest);

// ============================================================
// Comparison operators
// ============================================================

class ComparisonTest : public DualEngineTest
{};

TEST_P(ComparisonTest, LessThan)
{
    EXPECT_TRUE(evalBool("3 < 5;"));
    EXPECT_FALSE(evalBool("5 < 3;"));
}

TEST_P(ComparisonTest, GreaterThan)
{
    EXPECT_TRUE(evalBool("5 > 3;"));
    EXPECT_FALSE(evalBool("3 > 5;"));
}

TEST_P(ComparisonTest, LessEqual)
{
    EXPECT_TRUE(evalBool("3 <= 3;"));
    EXPECT_TRUE(evalBool("3 <= 5;"));
    EXPECT_FALSE(evalBool("5 <= 3;"));
}

TEST_P(ComparisonTest, GreaterEqual)
{
    EXPECT_TRUE(evalBool("3 >= 3;"));
    EXPECT_TRUE(evalBool("5 >= 3;"));
    EXPECT_FALSE(evalBool("3 >= 5;"));
}

TEST_P(ComparisonTest, Equal)
{
    EXPECT_TRUE(evalBool("3 == 3;"));
    EXPECT_FALSE(evalBool("3 == 4;"));
}

TEST_P(ComparisonTest, NotEqual)
{
    EXPECT_TRUE(evalBool("3 ~= 4;"));
    EXPECT_FALSE(evalBool("3 ~= 3;"));
}

TEST_P(ComparisonTest, ArrayComparison)
{
    eval("r = [1 2 3] > 2;");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isLogical());
    const uint8_t *d = r->logicalData();
    EXPECT_EQ(d[0], 0);
    EXPECT_EQ(d[1], 0);
    EXPECT_EQ(d[2], 1);
}

INSTANTIATE_DUAL(ComparisonTest);

// ============================================================
// Type checking and conversion
// ============================================================

class TypeTest : public DualEngineTest
{};

TEST_P(TypeTest, IsEmpty)
{
    EXPECT_TRUE(evalBool("isempty([]);"));
    EXPECT_FALSE(evalBool("isempty(1);"));
    EXPECT_FALSE(evalBool("isempty([1 2]);"));
}

TEST_P(TypeTest, IsScalar)
{
    EXPECT_TRUE(evalBool("isscalar(42);"));
    EXPECT_FALSE(evalBool("isscalar([1 2]);"));
}

TEST_P(TypeTest, IsChar)
{
    EXPECT_TRUE(evalBool("ischar('hello');"));
    EXPECT_FALSE(evalBool("ischar(42);"));
}

TEST_P(TypeTest, Double)
{
    eval("r = double(true);");
    EXPECT_DOUBLE_EQ(getVar("r"), 1.0);
}

INSTANTIATE_DUAL(TypeTest);

// ============================================================
// Edge cases
// ============================================================

class EdgeCaseTest : public DualEngineTest
{};

TEST_P(EdgeCaseTest, EmptyFor)
{
    // for i = 5:1 — empty range, body never executes
    eval("x = 42; for i = 5:1, x = 0; end");
    EXPECT_DOUBLE_EQ(getVar("x"), 42.0);
}

TEST_P(EdgeCaseTest, SingleElementMatrix)
{
    eval("A = [42];");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ(A->toScalar(), 42.0);
}

TEST_P(EdgeCaseTest, NestedParens)
{
    EXPECT_DOUBLE_EQ(evalScalar("((((5))));"), 5.0);
}

TEST_P(EdgeCaseTest, MultipleStatementsSameLine)
{
    eval("a = 1; b = 2; c = a + b;");
    EXPECT_DOUBLE_EQ(getVar("c"), 3.0);
}

TEST_P(EdgeCaseTest, TrailingSemicolons)
{
    eval("x = 10;;;");
    EXPECT_DOUBLE_EQ(getVar("x"), 10.0);
}

TEST_P(EdgeCaseTest, InfAndNanArithmetic)
{
    EXPECT_TRUE(std::isinf(evalScalar("1/0;")));
    EXPECT_TRUE(std::isnan(evalScalar("0/0;")));
    EXPECT_TRUE(std::isnan(evalScalar("inf - inf;")));
    EXPECT_TRUE(std::isinf(evalScalar("inf + 1;")));
}

TEST_P(EdgeCaseTest, LargeLoop)
{
    eval(R"(
        s = 0;
        for i = 1:10000
            s = s + 1;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("s"), 10000.0);
}

TEST_P(EdgeCaseTest, DeepRecursion)
{
    eval(R"(
        function s = rsum(n)
            if n <= 0, s = 0;
            else, s = n + rsum(n-1); end
        end
    )");
    eval("r = rsum(40);");
    EXPECT_DOUBLE_EQ(getVar("r"), 820.0); // 40*41/2
}

TEST_P(EdgeCaseTest, ReturnFromFunction)
{
    eval(R"(
        function y = f(x)
            if x < 0
                y = -1;
                return;
            end
            y = 1;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("f(-5);"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("f(5);"), 1.0);
}

TEST_P(EdgeCaseTest, MultipleReturnPaths)
{
    eval(R"(
        function y = classify(x)
            if x > 0
                y = 1;
                return;
            elseif x < 0
                y = -1;
                return;
            end
            y = 0;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("classify(5);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("classify(-3);"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("classify(0);"), 0.0);
}

INSTANTIATE_DUAL(EdgeCaseTest);

// ============================================================
// Linspace
// ============================================================

class LinspaceTest : public DualEngineTest
{};

TEST_P(LinspaceTest, BasicLinspace)
{
    eval("v = linspace(0, 1, 5);");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 5u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 1.0);
    EXPECT_NEAR(v->doubleData()[2], 0.5, 1e-12);
}

TEST_P(LinspaceTest, LinspaceTwoPoints)
{
    eval("v = linspace(0, 10, 2);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 2u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 10.0);
}

TEST_P(LinspaceTest, LinspaceSinglePoint)
{
    eval("v = linspace(5, 5, 1);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 1u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 5.0);
}

INSTANTIATE_DUAL(LinspaceTest);