// tests/test_advanced.cpp — Advanced features: scope, closures, string ops,
//   nargin/nargout, comparison, type checks, edge cases
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

#include <functional>
#include <vector>

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

// ── Phase P1.5 SIMD comparison-ops coverage ──────────────────
//
// The SIMD compare kernels (MStdCompare_simd.cpp) write 1 LOGICAL byte
// per double lane via StoreMaskBits + per-lane bit-shift expand. Tests
// below exercise:
//   * VV (vector-vs-vector same shape)
//   * VS (vector-vs-scalar broadcast on the right)
//   * SV (scalar-vs-vector broadcast on the left)
//   * Sizes that cross the 4× unroll boundary (need n >= 16 for AVX2,
//     n >= 32 for AVX-512) plus the 1× SIMD partial and scalar tail.

TEST_P(ComparisonTest, LargeNVectorVsScalarGreater)
{
    // x = 1..1000; check x > 500 has exactly 500 trues. nnz(LOGICAL)
    // isn't a builtin yet so count via element scan in C++.
    eval("x = 1:1000; m = x > 500;");
    auto *m = getVarPtr("m");
    EXPECT_EQ(m->numel(), 1000u);
    size_t trues = 0;
    for (size_t i = 0; i < 1000; ++i) trues += (m->logicalData()[i] != 0);
    EXPECT_EQ(trues, 500u);
}

TEST_P(ComparisonTest, LargeNScalarVsVectorLess)
{
    // 500 < x for x = 1..1000 → also 500 trues. Catches SV-shape kernel.
    eval("x = 1:1000; m = 500 < x;");
    auto *m = getVarPtr("m");
    size_t trues = 0;
    for (size_t i = 0; i < 1000; ++i) trues += (m->logicalData()[i] != 0);
    EXPECT_EQ(trues, 500u);
}

TEST_P(ComparisonTest, LargeNVectorVsVectorEqual)
{
    // a == b where they differ at known positions (1-based: 7, 100, 999).
    eval("a = 1:1000; b = a; b([7 100 999]) = -1; m = a == b;");
    auto *m = getVarPtr("m");
    size_t trues = 0;
    for (size_t i = 0; i < 1000; ++i) trues += (m->logicalData()[i] != 0);
    EXPECT_EQ(trues, 997u);
    // Specific positions must be FALSE (0-based: 6, 99, 998).
    EXPECT_FALSE(m->logicalData()[6]   != 0);
    EXPECT_FALSE(m->logicalData()[99]  != 0);
    EXPECT_FALSE(m->logicalData()[998] != 0);
}

TEST_P(ComparisonTest, AllSixOpsLargeNAgreeWithScalar)
{
    // For each cmp op, verify the SIMD-vector result agrees element-wise
    // with the C++ scalar reference. Catches any cross-op regression in
    // the macro (incl. the IEEE-NaN special case for ~=).
    eval("rng(13); x = randn(256, 1); y = randn(256, 1); thr = 0.5;");
    auto *x = getVarPtr("x");
    auto *y = getVarPtr("y");
    ASSERT_EQ(x->numel(), 256u);
    ASSERT_EQ(y->numel(), 256u);

    struct Op { const char *m_op; std::function<bool(double, double)> ref; };
    const std::vector<Op> ops = {
        {">",  [](double a, double b){ return a >  b; }},
        {"<",  [](double a, double b){ return a <  b; }},
        {">=", [](double a, double b){ return a >= b; }},
        {"<=", [](double a, double b){ return a <= b; }},
        {"==", [](double a, double b){ return a == b; }},
        {"~=", [](double a, double b){ return a != b; }},
    };
    for (const auto &op : ops) {
        eval(std::string("mv = x ") + op.m_op + " y;");
        auto *mv = getVarPtr("mv");
        ASSERT_EQ(mv->numel(), 256u) << "VV op " << op.m_op;
        for (size_t i = 0; i < 256; ++i) {
            const bool expect = op.ref(x->doubleData()[i], y->doubleData()[i]);
            EXPECT_EQ(mv->logicalData()[i] != 0, expect)
                << "VV op " << op.m_op << " at i=" << i;
        }
        eval(std::string("mv = x ") + op.m_op + " thr;");
        auto *mv2 = getVarPtr("mv");
        for (size_t i = 0; i < 256; ++i) {
            const bool expect = op.ref(x->doubleData()[i], 0.5);
            EXPECT_EQ(mv2->logicalData()[i] != 0, expect)
                << "VS op " << op.m_op << " at i=" << i;
        }
    }
}

TEST_P(ComparisonTest, NaNComparisonsAlwaysFalse)
{
    // IEEE 754: NaN OP anything is false except != which is true.
    eval("x = [1 NaN 2 NaN 3]; "
         "lt = x < 5; gt = x > 0; eq = x == x; ne = x ~= x;");
    auto *lt = getVarPtr("lt");
    auto *gt = getVarPtr("gt");
    auto *eq = getVarPtr("eq");
    auto *ne = getVarPtr("ne");
    // NaN positions: 2, 4 (1-based) → indices 1, 3 (0-based).
    EXPECT_FALSE(lt->logicalData()[1] != 0);
    EXPECT_FALSE(lt->logicalData()[3] != 0);
    EXPECT_FALSE(gt->logicalData()[1] != 0);
    EXPECT_FALSE(gt->logicalData()[3] != 0);
    EXPECT_FALSE(eq->logicalData()[1] != 0);  // NaN == NaN is false
    EXPECT_FALSE(eq->logicalData()[3] != 0);
    EXPECT_TRUE (ne->logicalData()[1] != 0);  // NaN ~= NaN is true
    EXPECT_TRUE (ne->logicalData()[3] != 0);
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