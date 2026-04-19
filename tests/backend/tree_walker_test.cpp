// tests/optimization_regression_test.cpp
//
// Regression tests targeting every optimization fast path.
// Each test verifies correctness of an optimized code path
// by comparing results against known values.

#include "MEngine.hpp"
#include "MStdLibrary.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit::m::m;

class OptimizationRegressionTest : public ::testing::Test
{
public:
    Engine engine;
    std::string capturedOutput;

    void SetUp() override
    {
        StdLibrary::install(engine);
        capturedOutput.clear();
        engine.setOutputFunc([this](const std::string &s) { capturedOutput += s; });
    }

    MValue eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// ============================================================
// 1. tryEvalScalar — inline scalar evaluation
// ============================================================

// --- Inline builtins: verify each cachedBuiltinId gives correct results ---

TEST_F(OptimizationRegressionTest, InlineBuiltin_Mod)
{
    EXPECT_DOUBLE_EQ(evalScalar("mod(10, 3);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("mod(10, 5);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("mod(7, 2);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("mod(-7, 3);"), 2.0); // MATLAB mod: result has sign of divisor
}

TEST_F(OptimizationRegressionTest, InlineBuiltin_Abs)
{
    EXPECT_DOUBLE_EQ(evalScalar("abs(-5);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("abs(5);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("abs(0);"), 0.0);
}

TEST_F(OptimizationRegressionTest, InlineBuiltin_Floor)
{
    EXPECT_DOUBLE_EQ(evalScalar("floor(3.7);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("floor(-3.2);"), -4.0);
    EXPECT_DOUBLE_EQ(evalScalar("floor(5);"), 5.0);
}

TEST_F(OptimizationRegressionTest, InlineBuiltin_Ceil)
{
    EXPECT_DOUBLE_EQ(evalScalar("ceil(3.2);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("ceil(-3.7);"), -3.0);
}

TEST_F(OptimizationRegressionTest, InlineBuiltin_Round)
{
    EXPECT_DOUBLE_EQ(evalScalar("round(3.5);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("round(3.4);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("round(-2.5);"), -3.0);
}

TEST_F(OptimizationRegressionTest, InlineBuiltin_Fix)
{
    eval("x = fix(3.7);");
    EXPECT_DOUBLE_EQ(evalScalar("x;"), 3.0);
    eval("y = fix(-3.7);");
    EXPECT_DOUBLE_EQ(evalScalar("y;"), -3.0);
}

TEST_F(OptimizationRegressionTest, InlineBuiltin_SinCosTan)
{
    EXPECT_NEAR(evalScalar("sin(pi/2);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("cos(0);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("cos(pi);"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("tan(0);"), 0.0, 1e-12);
}

TEST_F(OptimizationRegressionTest, InlineBuiltin_Sqrt)
{
    EXPECT_DOUBLE_EQ(evalScalar("sqrt(16);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("sqrt(0);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("sqrt(1);"), 1.0);
}

TEST_F(OptimizationRegressionTest, InlineBuiltin_ExpLog)
{
    EXPECT_NEAR(evalScalar("exp(0);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("exp(1);"), std::exp(1.0), 1e-12);
    EXPECT_NEAR(evalScalar("log(1);"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("log(exp(3));"), 3.0, 1e-12);
}

TEST_F(OptimizationRegressionTest, InlineBuiltin_Log2Log10)
{
    EXPECT_DOUBLE_EQ(evalScalar("log2(8);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("log10(1000);"), 3.0);
}

TEST_F(OptimizationRegressionTest, InlineBuiltin_MinMaxScalar)
{
    EXPECT_DOUBLE_EQ(evalScalar("min(3, 5);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(3, 5);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("min(-1, 1);"), -1.0);
}

TEST_F(OptimizationRegressionTest, InlineBuiltin_Sign)
{
    EXPECT_DOUBLE_EQ(evalScalar("sign(5);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sign(-3);"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sign(0);"), 0.0);
}

// --- Inline builtins in loop context (cached builtin ID reuse) ---

TEST_F(OptimizationRegressionTest, InlineBuiltinInLoop)
{
    eval(R"(
        s = 0;
        for i = 1:100
            s = s + mod(i, 7);
        end
    )");
    // sum of mod(1:100, 7)
    double expected = 0;
    for (int i = 1; i <= 100; ++i)
        expected += std::fmod(i, 7);
    EXPECT_DOUBLE_EQ(evalScalar("s;"), expected);
}

TEST_F(OptimizationRegressionTest, InlineBuiltinSinInLoop)
{
    eval(R"(
        s = 0;
        for i = 1:50
            s = s + sin(i * 0.1);
        end
    )");
    double expected = 0;
    for (int i = 1; i <= 50; ++i)
        expected += std::sin(i * 0.1);
    EXPECT_NEAR(evalScalar("s;"), expected, 1e-10);
}

// --- Array scalar indexing in tryEvalScalar ---

TEST_F(OptimizationRegressionTest, ArrayScalarRead1D)
{
    eval("A = [10 20 30 40 50];");
    eval("x = A(3);");
    EXPECT_DOUBLE_EQ(evalScalar("x;"), 30.0);
}

TEST_F(OptimizationRegressionTest, ArrayScalarRead1DInExpression)
{
    eval("A = [10 20 30]; x = A(1) + A(2) + A(3);");
    EXPECT_DOUBLE_EQ(evalScalar("x;"), 60.0);
}

TEST_F(OptimizationRegressionTest, ArrayScalarRead2D)
{
    eval("M = [1 2 3; 4 5 6; 7 8 9];");
    EXPECT_DOUBLE_EQ(evalScalar("M(2, 3);"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(3, 1);"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(1, 1);"), 1.0);
}

TEST_F(OptimizationRegressionTest, ArrayScalarRead2DInLoop)
{
    eval(R"(
        M = [1 2 3; 4 5 6; 7 8 9];
        s = 0;
        for i = 1:3
            for j = 1:3
                s = s + M(i, j);
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("s;"), 45.0);
}

// --- Struct field read in tryEvalScalar ---

TEST_F(OptimizationRegressionTest, StructFieldScalarRead)
{
    eval("p.x = 10; p.y = 20;");
    EXPECT_DOUBLE_EQ(evalScalar("p.x;"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("p.y;"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("p.x + p.y;"), 30.0);
}

TEST_F(OptimizationRegressionTest, StructFieldInLoop)
{
    eval(R"(
        p.x = 0;
        p.y = 0;
        for i = 1:100
            p.x = p.x + 1;
            p.y = p.y + p.x;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("p.x;"), 100.0);
    EXPECT_DOUBLE_EQ(evalScalar("p.y;"), 5050.0);
}

// ============================================================
// 2. execBlock fast paths
// ============================================================

TEST_F(OptimizationRegressionTest, ScalarAssignFastPath)
{
    eval(R"(
        x = 1;
        for i = 1:1000
            x = x + 1;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("x;"), 1001.0);
}

TEST_F(OptimizationRegressionTest, IndexedAssign1DFastPath)
{
    eval(R"(
        A = zeros(1, 100);
        for i = 1:100
            A(i) = i * 2;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(50);"), 100.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(100);"), 200.0);
}

TEST_F(OptimizationRegressionTest, IndexedAssign2DFastPath)
{
    eval(R"(
        M = zeros(5, 5);
        for i = 1:5
            for j = 1:5
                M(i, j) = i * 10 + j;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("M(1,1);"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(3,4);"), 34.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(5,5);"), 55.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(sum(M));"), 825.0);
}

TEST_F(OptimizationRegressionTest, StructFieldAssignFastPath)
{
    eval(R"(
        s.val = 0;
        for i = 1:50
            s.val = s.val + i;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("s.val;"), 1275.0);
}

TEST_F(OptimizationRegressionTest, MatrixFillWithBuiltins)
{
    eval(R"(
        M = zeros(10, 10);
        for i = 1:10
            for j = 1:10
                M(i, j) = sin(i * 0.1) * cos(j * 0.1);
            end
        end
    )");
    double expected = std::sin(5 * 0.1) * std::cos(5 * 0.1);
    EXPECT_NEAR(evalScalar("M(5, 5);"), expected, 1e-12);
}

// ============================================================
// 3. FlowSignal — break/continue/return without exceptions
// ============================================================

TEST_F(OptimizationRegressionTest, BreakInForLoop)
{
    eval(R"(
        s = 0;
        for i = 1:100
            if i > 10
                break;
            end
            s = s + i;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("s;"), 55.0);
}

TEST_F(OptimizationRegressionTest, ContinueInForLoop)
{
    eval(R"(
        s = 0;
        for i = 1:10
            if mod(i, 2) == 0
                continue;
            end
            s = s + i;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("s;"), 25.0); // 1+3+5+7+9
}

TEST_F(OptimizationRegressionTest, BreakInWhileLoop)
{
    eval(R"(
        x = 0;
        while true
            x = x + 1;
            if x >= 5
                break;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("x;"), 5.0);
}

TEST_F(OptimizationRegressionTest, ContinueInWhileLoop)
{
    eval(R"(
        s = 0;
        i = 0;
        while i < 10
            i = i + 1;
            if mod(i, 3) == 0
                continue;
            end
            s = s + i;
        end
    )");
    // Skip 3,6,9: 1+2+4+5+7+8+10 = 37
    EXPECT_DOUBLE_EQ(evalScalar("s;"), 37.0);
}

TEST_F(OptimizationRegressionTest, ReturnFromFunction)
{
    eval(R"(
        function r = early_ret(x)
            if x < 0
                r = -1;
                return;
            end
            r = x * 2;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("early_ret(-5);"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("early_ret(3);"), 6.0);
}

TEST_F(OptimizationRegressionTest, NestedLoopBreak)
{
    eval(R"(
        count = 0;
        for i = 1:5
            for j = 1:5
                if j == 3
                    break;
                end
                count = count + 1;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("count;"), 10.0); // 5 outer * 2 inner
}

TEST_F(OptimizationRegressionTest, NestedLoopContinue)
{
    eval(R"(
        s = 0;
        for i = 1:5
            for j = 1:5
                if mod(j, 2) == 0
                    continue;
                end
                s = s + 1;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("s;"), 15.0); // 5 outer * 3 odd j's
}

TEST_F(OptimizationRegressionTest, ReturnInsideLoop)
{
    eval(R"(
        function r = find_first(A, target)
            for i = 1:length(A)
                if A(i) == target
                    r = i;
                    return;
                end
            end
            r = -1;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("find_first([5 3 8 1 9], 8);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("find_first([5 3 8 1 9], 7);"), -1.0);
}

TEST_F(OptimizationRegressionTest, ReturnInsideTryCatch)
{
    eval(R"(
        function r = safe_div(a, b)
            try
                if b == 0
                    r = inf;
                    return;
                end
                r = a / b;
            catch
                r = nan;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("safe_div(10, 2);"), 5.0);
    EXPECT_TRUE(std::isinf(evalScalar("safe_div(1, 0);")));
}

TEST_F(OptimizationRegressionTest, BreakInsideTryCatch)
{
    eval(R"(
        s = 0;
        for i = 1:10
            try
                if i > 5
                    break;
                end
                s = s + i;
            catch
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("s;"), 15.0);
}

// ============================================================
// 4. Environment — stack allocation and SBO
// ============================================================

TEST_F(OptimizationRegressionTest, FunctionLocalScope)
{
    eval("x = 100;");
    eval(R"(
        function r = local_test()
            x = 5;
            r = x;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("local_test();"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("x;"), 100.0);
}

TEST_F(OptimizationRegressionTest, ManyLocalVariables)
{
    // Tests SBO overflow (>8 variables in function scope)
    eval(R"(
        function r = many_vars()
            a = 1; b = 2; c = 3; d = 4;
            e = 5; f = 6; g = 7; h = 8;
            i = 9; j = 10;
            r = a + b + c + d + e + f + g + h + i + j;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("many_vars();"), 55.0);
}

TEST_F(OptimizationRegressionTest, RecursionWithStackEnv)
{
    eval(R"(
        function r = fact(n)
            if n <= 1
                r = 1;
            else
                r = n * fact(n - 1);
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("fact(10);"), 3628800.0);
}

TEST_F(OptimizationRegressionTest, ClosureCapture)
{
    eval(R"(
        a = 10;
        f = @(x) x + a;
        a = 100;
    )");
    // Closure captures a=10, not a=100
    EXPECT_DOUBLE_EQ(evalScalar("f(5);"), 15.0);
}

TEST_F(OptimizationRegressionTest, ClosureInFunction)
{
    eval(R"(
        function h = make_adder(n)
            h = @(x) x + n;
        end
    )");
    eval("add5 = make_adder(5);");
    EXPECT_DOUBLE_EQ(evalScalar("add5(10);"), 15.0);
    eval("add3 = make_adder(3);");
    EXPECT_DOUBLE_EQ(evalScalar("add3(10);"), 13.0);
    // add5 still works
    EXPECT_DOUBLE_EQ(evalScalar("add5(10);"), 15.0);
}

// ============================================================
// 5. Multi-output nargout guard
// ============================================================

TEST_F(OptimizationRegressionTest, MinSingleOutput)
{
    // min(A) with nargout=1 — should not crash
    EXPECT_DOUBLE_EQ(evalScalar("min([3 1 4 1 5]);"), 1.0);
}

TEST_F(OptimizationRegressionTest, MinDoubleOutput)
{
    eval("[mn, idx] = min([3 1 4 1 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("mn;"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx;"), 2.0);
}

TEST_F(OptimizationRegressionTest, MaxSingleOutput)
{
    EXPECT_DOUBLE_EQ(evalScalar("max([3 1 4 1 5]);"), 5.0);
}

TEST_F(OptimizationRegressionTest, MaxDoubleOutput)
{
    eval("[mx, idx] = max([3 1 4 1 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("mx;"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx;"), 5.0);
}

TEST_F(OptimizationRegressionTest, SizeSingleOutput)
{
    eval("A = ones(3, 4);");
    eval("n = size(A, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("n;"), 3.0);
}

TEST_F(OptimizationRegressionTest, SizeDoubleOutput)
{
    eval("[r, c] = size(ones(3, 4));");
    EXPECT_DOUBLE_EQ(evalScalar("r;"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c;"), 4.0);
}

// ============================================================
// 6. Cached UserFunction pointer
// ============================================================

TEST_F(OptimizationRegressionTest, CachedUserFuncBasic)
{
    eval(R"(
        function r = double_it(x)
            r = x * 2;
        end
    )");
    // First call caches, subsequent calls use cache
    EXPECT_DOUBLE_EQ(evalScalar("double_it(5);"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("double_it(7);"), 14.0);
    EXPECT_DOUBLE_EQ(evalScalar("double_it(0);"), 0.0);
}

TEST_F(OptimizationRegressionTest, FunctionRedefinitionInvalidatesCache)
{
    eval(R"(
        function r = f(x)
            r = x + 1;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("f(5);"), 6.0);

    // Redefine
    eval(R"(
        function r = f(x)
            r = x * 2;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("f(5);"), 10.0);
}

TEST_F(OptimizationRegressionTest, RecursiveCachedFunc)
{
    eval(R"(
        function r = fib(n)
            if n <= 1
                r = n;
            else
                r = fib(n-1) + fib(n-2);
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("fib(10);"), 55.0);
    EXPECT_DOUBLE_EQ(evalScalar("fib(0);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("fib(1);"), 1.0);
}

// ============================================================
// 7. Lazy nargin/nargout
// ============================================================

TEST_F(OptimizationRegressionTest, FunctionWithoutNarginNargout)
{
    // Simple function — nargin/nargout not used, should not be set
    eval(R"(
        function r = add(a, b)
            r = a + b;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("add(3, 4);"), 7.0);
}

TEST_F(OptimizationRegressionTest, FunctionWithNargin)
{
    eval(R"(
        function r = flexible(a, b)
            if nargin == 1
                r = a * 2;
            else
                r = a + b;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("flexible(5);"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("flexible(3, 4);"), 7.0);
}

TEST_F(OptimizationRegressionTest, FunctionWithNargout)
{
    eval(R"(
        function [a, b] = maybe_two(x)
            a = x;
            if nargout > 1
                b = x * 2;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("maybe_two(5);"), 5.0);
    eval("[p, q] = maybe_two(5);");
    EXPECT_DOUBLE_EQ(evalScalar("p;"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("q;"), 10.0);
}

// ============================================================
// 8. Inline builtins in if/while conditions
// ============================================================

TEST_F(OptimizationRegressionTest, ModInIfCondition)
{
    eval(R"(
        count = 0;
        for i = 1:30
            if mod(i, 3) == 0
                count = count + 1;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("count;"), 10.0);
}

TEST_F(OptimizationRegressionTest, AbsInIfCondition)
{
    eval(R"(
        count = 0;
        for i = -5:5
            if abs(i) > 3
                count = count + 1;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("count;"), 4.0); // -5,-4,4,5
}

TEST_F(OptimizationRegressionTest, FloorInWhileCondition)
{
    eval(R"(
        x = 10.5;
        count = 0;
        while floor(x) > 0
            x = x / 2;
            count = count + 1;
        end
    )");
    // 10.5 → 5.25 → 2.625 → 1.3125 → 0.65625 (floor=0, stop)
    EXPECT_DOUBLE_EQ(evalScalar("count;"), 4.0);
}

TEST_F(OptimizationRegressionTest, ComplexConditionWithBuiltins)
{
    eval(R"(
        c1 = 0; c2 = 0; c3 = 0;
        for i = 1:100
            if mod(i, 3) == 0
                c1 = c1 + 1;
            elseif mod(i, 5) == 0
                c2 = c2 + 1;
            else
                c3 = c3 + 1;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("c1;"), 33.0); // divisible by 3
    EXPECT_DOUBLE_EQ(evalScalar("c2;"), 14.0); // divisible by 5 but not 3
    EXPECT_DOUBLE_EQ(evalScalar("c3;"), 53.0); // rest
}

// ============================================================
// 9. For-loop in-place iterator update
// ============================================================

TEST_F(OptimizationRegressionTest, ForLoopScalarIterator)
{
    eval(R"(
        s = 0;
        for i = 1:100
            s = s + i;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("s;"), 5050.0);
}

TEST_F(OptimizationRegressionTest, ForLoopSteppedRange)
{
    eval(R"(
        s = 0;
        for i = 0:2:10
            s = s + i;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("s;"), 30.0); // 0+2+4+6+8+10
}

TEST_F(OptimizationRegressionTest, ForLoopNegativeStep)
{
    eval(R"(
        s = 0;
        for i = 10:-1:1
            s = s + i;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("s;"), 55.0);
}

TEST_F(OptimizationRegressionTest, ForLoopOverVector)
{
    eval(R"(
        s = 0;
        for i = [3 1 4 1 5]
            s = s + i;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("s;"), 14.0);
}

TEST_F(OptimizationRegressionTest, ForLoopOverMatrixColumns)
{
    eval(R"(
        A = [1 2 3; 4 5 6; 7 8 9];
        s = 0;
        for col = A
            s = s + sum(col);
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("s;"), 45.0);
}

// ============================================================
// 10. Amortized array append
// ============================================================

TEST_F(OptimizationRegressionTest, ArrayGrowAppend)
{
    eval(R"(
        A = [];
        for i = 1:100
            A = [A, i];
        end
    )");
    EXPECT_EQ(eval("A").numel(), 100u);
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(100);"), 100.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(A);"), 5050.0);
}

// ============================================================
// 11. Edge cases and mixed paths
// ============================================================

TEST_F(OptimizationRegressionTest, MixedScalarAndArrayOps)
{
    eval(R"(
        A = [1 2 3 4 5];
        s = 0;
        for i = 1:length(A)
            s = s + A(i) * sin(i);
        end
    )");
    double expected = 0;
    double A[] = {1, 2, 3, 4, 5};
    for (int i = 1; i <= 5; ++i)
        expected += A[i - 1] * std::sin(i);
    EXPECT_NEAR(evalScalar("s;"), expected, 1e-10);
}

TEST_F(OptimizationRegressionTest, FunctionCallingFunction)
{
    eval(R"(
        function r = double_it(x)
            r = x * 2;
        end
        function r = quad(x)
            r = double_it(double_it(x));
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("quad(3);"), 12.0);
}

TEST_F(OptimizationRegressionTest, NestedForWithBreakContinue)
{
    eval(R"(
        result = [];
        for i = 1:10
            if mod(i, 2) == 0
                continue;
            end
            if i > 7
                break;
            end
            result = [result, i];
        end
    )");
    auto r = eval("result;");
    EXPECT_EQ(r.numel(), 4u); // 1, 3, 5, 7
    EXPECT_DOUBLE_EQ(r.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(r.doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(r.doubleData()[2], 5.0);
    EXPECT_DOUBLE_EQ(r.doubleData()[3], 7.0);
}

TEST_F(OptimizationRegressionTest, GlobalVariableInFunction)
{
    eval(R"(
        global g_counter;
        g_counter = 0;
        function increment()
            global g_counter;
            g_counter = g_counter + 1;
        end
    )");
    eval("increment(); increment(); increment();");
    EXPECT_DOUBLE_EQ(evalScalar("g_counter;"), 3.0);
}

TEST_F(OptimizationRegressionTest, TryCatchDoesNotSwallowFlowSignals)
{
    // break/continue/return should propagate through try/catch
    eval(R"(
        s = 0;
        for i = 1:10
            try
                if i > 5
                    break;
                end
            catch
            end
            s = s + i;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("s;"), 15.0); // 1+2+3+4+5

    eval(R"(
        function r = ret_in_try()
            try
                r = 42;
                return;
            catch
            end
            r = -1;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("ret_in_try();"), 42.0);
}

TEST_F(OptimizationRegressionTest, SwitchStillWorks)
{
    eval(R"(
        function r = classify(x)
            switch x
                case 1
                    r = 10;
                case {2, 3}
                    r = 20;
                otherwise
                    r = 0;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("classify(1);"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("classify(2);"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("classify(3);"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("classify(99);"), 0.0);
}

TEST_F(OptimizationRegressionTest, ErrorStillThrows)
{
    EXPECT_THROW(eval("error('test error');"), std::runtime_error);
}

TEST_F(OptimizationRegressionTest, UndefinedVariableStillThrows)
{
    EXPECT_THROW(eval("nonexistent_var;"), std::runtime_error);
}

TEST_F(OptimizationRegressionTest, IndexOutOfBoundsStillThrows)
{
    eval("A = [1 2 3];");
    EXPECT_THROW(eval("A(10);"), std::runtime_error);
}