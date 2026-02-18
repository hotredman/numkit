#include "MLabEngine.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

using namespace mlab;

class EngineAdvancedTest : public ::testing::Test
{
protected:
    Engine engine;
    std::string captured_output;

    void SetUp() override
    {
        captured_output.clear();
        engine.setOutputFunc([this](const std::string &s) { captured_output += s; });
    }

    MValue eval(const std::string &code) { return engine.eval(code); }

    double toDouble(const MValue &v) { return v.toScalar(); }

    void expectElem(const MValue &val, size_t i, double expected)
    {
        EXPECT_DOUBLE_EQ(val(i), expected) << "at linear index " << i;
    }

    void expectElem2D(const MValue &val, size_t r, size_t c, double expected)
    {
        EXPECT_DOUBLE_EQ(val(r, c), expected) << "at (" << r << "," << c << ")";
    }

    void expectNearElem2D(const MValue &val, size_t r, size_t c, double expected, double tol)
    {
        EXPECT_NEAR(val(r, c), expected, tol) << "at (" << r << "," << c << ")";
    }

    size_t rows(const MValue &v) { return v.dims().rows(); }
    size_t cols(const MValue &v) { return v.dims().cols(); }
};

// =============================================================================
// Рекурсия
// =============================================================================

TEST_F(EngineAdvancedTest, RecursiveFactorial)
{
    eval(R"(
        function r = factorial_rec(n)
            if n <= 1
                r = 1;
            else
                r = n * factorial_rec(n - 1);
            end
        end
    )");
    MValue result = eval("factorial_rec(10);");
    EXPECT_DOUBLE_EQ(toDouble(result), 3628800.0);
}

TEST_F(EngineAdvancedTest, RecursiveFibonacci)
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
    EXPECT_DOUBLE_EQ(toDouble(eval("fib(0);")), 0.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("fib(1);")), 1.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("fib(10);")), 55.0);
}

TEST_F(EngineAdvancedTest, MutualRecursion)
{
    eval(R"(
        function r = is_even(n)
            if n == 0
                r = 1;
            else
                r = is_odd(n - 1);
            end
        end

        function r = is_odd(n)
            if n == 0
                r = 0;
            else
                r = is_even(n - 1);
            end
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("is_even(10);")), 1.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("is_even(7);")), 0.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("is_odd(7);")), 1.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("is_odd(4);")), 0.0);
}

TEST_F(EngineAdvancedTest, RecursionDepthExceeded)
{
    engine.setMaxRecursionDepth(50);
    eval(R"(
        function r = infinite_rec(n)
            r = infinite_rec(n + 1);
        end
    )");
    EXPECT_THROW(eval("infinite_rec(0);"), std::runtime_error);
}

TEST_F(EngineAdvancedTest, TailRecursiveSum)
{
    eval(R"(
        function r = sum_to(n, acc)
            if n == 0
                r = acc;
            else
                r = sum_to(n - 1, acc + n);
            end
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("sum_to(100, 0);")), 5050.0);
}

// =============================================================================
// Вложенные циклы и управление потоком
// =============================================================================

TEST_F(EngineAdvancedTest, NestedForLoops)
{
    eval(R"(
        result = zeros(3, 3);
        for i = 1:3
            for j = 1:3
                result(i, j) = i * 10 + j;
            end
        end
    )");
    MValue result = eval("result;");
    expectElem2D(result, 0, 0, 11);
    expectElem2D(result, 0, 2, 13);
    expectElem2D(result, 2, 2, 33);
    expectElem2D(result, 1, 1, 22);
}

TEST_F(EngineAdvancedTest, BreakInNestedLoop)
{
    eval(R"(
        count = 0;
        for i = 1:10
            for j = 1:10
                if j == 3
                    break;
                end
                count = count + 1;
            end
        end
    )");
    // Inner loop breaks at j==3, so each outer iteration does j=1,2 => 2 iterations
    // 10 outer * 2 inner = 20
    EXPECT_DOUBLE_EQ(toDouble(eval("count;")), 20.0);
}

TEST_F(EngineAdvancedTest, ContinueInLoop)
{
    eval(R"(
        total = 0;
        for i = 1:10
            if mod(i, 3) == 0
                continue;
            end
            total = total + i;
        end
    )");
    // Skip 3, 6, 9 => 1+2+4+5+7+8+10 = 37
    EXPECT_DOUBLE_EQ(toDouble(eval("total;")), 37.0);
}

TEST_F(EngineAdvancedTest, WhileWithBreak)
{
    eval(R"(
        x = 1;
        while true
            if x > 100
                break;
            end
            x = x * 2;
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 128.0);
}

TEST_F(EngineAdvancedTest, NestedWhileLoops)
{
    eval(R"(
        result = 0;
        i = 1;
        while i <= 5
            j = 1;
            while j <= i
                result = result + 1;
                j = j + 1;
            end
            i = i + 1;
        end
    )");
    // 1+2+3+4+5 = 15
    EXPECT_DOUBLE_EQ(toDouble(eval("result;")), 15.0);
}

TEST_F(EngineAdvancedTest, ForLoopOverMatrixColumns)
{
    eval(R"(
        A = [1 2 3; 4 5 6; 7 8 9];
        s = 0;
        for col = A
            s = s + sum(col);
        end
    )");
    // sum of all elements: 45
    EXPECT_DOUBLE_EQ(toDouble(eval("s;")), 45.0);
}

// =============================================================================
// Матричные операции
// =============================================================================

TEST_F(EngineAdvancedTest, MatrixMultiplication)
{
    eval(R"(
        A = [1 2; 3 4];
        B = [5 6; 7 8];
        C = A * B;
    )");
    MValue C = eval("C;");
    expectElem2D(C, 0, 0, 19); // 1*5+2*7
    expectElem2D(C, 0, 1, 22); // 1*6+2*8
    expectElem2D(C, 1, 0, 43); // 3*5+4*7
    expectElem2D(C, 1, 1, 50); // 3*6+4*8
}

TEST_F(EngineAdvancedTest, MatrixTranspose)
{
    eval(R"(
        A = [1 2 3; 4 5 6];
        B = A';
    )");
    MValue B = eval("B;");
    EXPECT_EQ(rows(B), 3u);
    EXPECT_EQ(cols(B), 2u);
    expectElem2D(B, 0, 0, 1);
    expectElem2D(B, 0, 1, 4);
    expectElem2D(B, 2, 0, 3);
    expectElem2D(B, 2, 1, 6);
}

TEST_F(EngineAdvancedTest, ElementWiseOperations)
{
    eval(R"(
        A = [1 2 3; 4 5 6];
        B = [7 8 9; 10 11 12];
        C = A .* B;
        D = A ./ B;
        E = A .^ 2;
    )");
    MValue C = eval("C;");
    expectElem2D(C, 0, 0, 7);
    expectElem2D(C, 1, 2, 72);

    MValue E = eval("E;");
    expectElem2D(E, 0, 0, 1);
    expectElem2D(E, 0, 1, 4);
    expectElem2D(E, 1, 2, 36);
}

TEST_F(EngineAdvancedTest, MatrixConcatenation)
{
    eval(R"(
        A = [1 2; 3 4];
        B = [5 6; 7 8];
        H = [A, B];
        V = [A; B];
    )");
    MValue H = eval("H;");
    EXPECT_EQ(rows(H), 2u);
    EXPECT_EQ(cols(H), 4u);
    expectElem2D(H, 0, 2, 5);
    expectElem2D(H, 1, 3, 8);

    MValue V = eval("V;");
    EXPECT_EQ(rows(V), 4u);
    EXPECT_EQ(cols(V), 2u);
    expectElem2D(V, 2, 0, 5);
    expectElem2D(V, 3, 1, 8);
}

TEST_F(EngineAdvancedTest, MatrixSlicing)
{
    eval(R"(
        A = [1 2 3 4; 5 6 7 8; 9 10 11 12];
        B = A(1:2, 2:3);
    )");
    MValue B = eval("B;");
    EXPECT_EQ(rows(B), 2u);
    EXPECT_EQ(cols(B), 2u);
    expectElem2D(B, 0, 0, 2);
    expectElem2D(B, 0, 1, 3);
    expectElem2D(B, 1, 0, 6);
    expectElem2D(B, 1, 1, 7);
}

TEST_F(EngineAdvancedTest, MatrixLinearIndexing)
{
    eval(R"(
        A = [1 4 7; 2 5 8; 3 6 9];
        x = A(5);
    )");
    // Column-major linear indexing: A(5) = 5
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 5.0);
}

TEST_F(EngineAdvancedTest, MatrixLogicalIndexing)
{
    eval(R"(
        A = [1 2 3 4 5 6 7 8 9 10];
        B = A(A > 5);
    )");
    MValue B = eval("B;");
    // Should be [6 7 8 9 10]
    EXPECT_EQ(B.numel(), 5u);
}

TEST_F(EngineAdvancedTest, ColonAsFullIndex)
{
    eval(R"(
        A = [1 2 3; 4 5 6; 7 8 9];
        B = A(:, 2);
    )");
    MValue B = eval("B;");
    EXPECT_EQ(rows(B), 3u);
    EXPECT_EQ(cols(B), 1u);
    expectElem2D(B, 0, 0, 2);
    expectElem2D(B, 1, 0, 5);
    expectElem2D(B, 2, 0, 8);
}

TEST_F(EngineAdvancedTest, MatrixDeletion)
{
    eval(R"(
        A = [1 2 3 4 5];
        A(3) = [];
    )");
    MValue A = eval("A;");
    EXPECT_EQ(A.numel(), 4u);
}

TEST_F(EngineAdvancedTest, DynamicMatrixGrowth)
{
    eval(R"(
        A = [];
        for i = 1:5
            A = [A, i^2];
        end
    )");
    MValue A = eval("A;");
    EXPECT_EQ(A.numel(), 5u);
    expectElem(A, 0, 1);
    expectElem(A, 1, 4);
    expectElem(A, 2, 9);
    expectElem(A, 3, 16);
    expectElem(A, 4, 25);
}

TEST_F(EngineAdvancedTest, MatrixAutoExpand)
{
    eval(R"(
        A = zeros(2, 2);
        A(5, 5) = 99;
    )");
    MValue A = eval("A;");
    EXPECT_EQ(rows(A), 5u);
    EXPECT_EQ(cols(A), 5u);
    expectElem2D(A, 4, 4, 99);
    expectElem2D(A, 0, 0, 0);
}

// =============================================================================
// Colon expressions
// =============================================================================

TEST_F(EngineAdvancedTest, ColonWithStep)
{
    eval("x = 0:0.5:2;");
    MValue x = eval("x;");
    EXPECT_EQ(x.numel(), 5u);
    expectElem(x, 0, 0.0);
    expectElem(x, 1, 0.5);
    expectElem(x, 4, 2.0);
}

TEST_F(EngineAdvancedTest, ColonDescending)
{
    eval("x = 5:-1:1;");
    MValue x = eval("x;");
    EXPECT_EQ(x.numel(), 5u);
    expectElem(x, 0, 5.0);
    expectElem(x, 4, 1.0);
}

TEST_F(EngineAdvancedTest, ColonEmptyRange)
{
    eval("x = 5:1:1;");
    MValue x = eval("x;");
    EXPECT_EQ(x.numel(), 0u);
}

// =============================================================================
// Функции с несколькими выходами
// =============================================================================

TEST_F(EngineAdvancedTest, MultipleReturnValues)
{
    eval(R"(
        function [mn, mx] = minmax(v)
            mn = min(v);
            mx = max(v);
        end
    )");
    eval("[a, b] = minmax([3 1 4 1 5 9 2 6]);");
    EXPECT_DOUBLE_EQ(toDouble(eval("a;")), 1.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("b;")), 9.0);
}

TEST_F(EngineAdvancedTest, MultiReturnPartialCapture)
{
    eval(R"(
        function [a, b, c] = triple(x)
            a = x;
            b = x * 2;
            c = x * 3;
        end
    )");
    eval("[p, q] = triple(5);");
    EXPECT_DOUBLE_EQ(toDouble(eval("p;")), 5.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("q;")), 10.0);
}

TEST_F(EngineAdvancedTest, MultiReturnInExpression)
{
    eval(R"(
        function [s, p] = sum_prod(a, b)
            s = a + b;
            p = a * b;
        end
    )");
    eval("[s, p] = sum_prod(3, 7);");
    EXPECT_DOUBLE_EQ(toDouble(eval("s;")), 10.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("p;")), 21.0);
}

// =============================================================================
// Анонимные функции и замыкания
// =============================================================================

TEST_F(EngineAdvancedTest, BasicAnonymousFunction)
{
    eval("f = @(x) x^2 + 1;");
    EXPECT_DOUBLE_EQ(toDouble(eval("f(3);")), 10.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("f(0);")), 1.0);
}

TEST_F(EngineAdvancedTest, AnonymousFunctionClosure)
{
    eval(R"(
        a = 10;
        f = @(x) x + a;
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("f(5);")), 15.0);

    // Changing a should NOT affect the closure (MATLAB captures by value)
    eval("a = 100;");
    EXPECT_DOUBLE_EQ(toDouble(eval("f(5);")), 15.0);
}

TEST_F(EngineAdvancedTest, AnonymousFunctionMultipleArgs)
{
    eval("g = @(x, y) x^2 + y^2;");
    EXPECT_DOUBLE_EQ(toDouble(eval("g(3, 4);")), 25.0);
}

TEST_F(EngineAdvancedTest, HigherOrderFunction)
{
    eval(R"(
        function r = apply(f, x)
            r = f(x);
        end
    )");
    eval("sq = @(x) x^2;");
    EXPECT_DOUBLE_EQ(toDouble(eval("apply(sq, 7);")), 49.0);
}

TEST_F(EngineAdvancedTest, FunctionReturningAnonymousFunc)
{
    eval(R"(
        function h = make_adder(n)
            h = @(x) x + n;
        end
    )");
    eval("add5 = make_adder(5);");
    EXPECT_DOUBLE_EQ(toDouble(eval("add5(10);")), 15.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("add5(0);")), 5.0);
}

TEST_F(EngineAdvancedTest, CompositionOfAnonymousFunctions)
{
    eval(R"(
        compose = @(f, g) @(x) f(g(x));
        double_it = @(x) x * 2;
        add_one = @(x) x + 1;
        double_then_add = compose(add_one, double_it);
    )");
    // double_then_add(5) = add_one(double_it(5)) = add_one(10) = 11
    EXPECT_DOUBLE_EQ(toDouble(eval("double_then_add(5);")), 11.0);
}

TEST_F(EngineAdvancedTest, FunctionHandle)
{
    eval(R"(
        function r = my_square(x)
            r = x * x;
        end
    )");
    eval("h = @my_square;");
    EXPECT_DOUBLE_EQ(toDouble(eval("h(6);")), 36.0);
}

// =============================================================================
// Cell arrays
// =============================================================================

TEST_F(EngineAdvancedTest, CellArrayCreation)
{
    eval("c = {1, 'hello', [1 2 3]};");
    MValue c = eval("c;");
    EXPECT_TRUE(c.isCell());
}

TEST_F(EngineAdvancedTest, CellArrayIndexing)
{
    eval(R"(
        c = {10, 20, 30};
        x = c{2};
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 20.0);
}

TEST_F(EngineAdvancedTest, CellArrayAssignment)
{
    eval(R"(
        c = {1, 2, 3};
        c{2} = 'updated';
    )");
    MValue c = eval("c;");
    EXPECT_TRUE(c.isCell());
}

TEST_F(EngineAdvancedTest, NestedCellArray)
{
    eval(R"(
        c = {1, {2, 3}, {4, {5, 6}}};
        x = c{2}{1};
        y = c{3}{2}{2};
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 2.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("y;")), 6.0);
}

TEST_F(EngineAdvancedTest, CellArrayInLoop)
{
    eval(R"(
        c = {};
        for i = 1:5
            c{i} = i^2;
        end
    )");
    eval("x = c{3};");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 9.0);
    eval("x = c{5};");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 25.0);
}

// =============================================================================
// Structs
// =============================================================================

TEST_F(EngineAdvancedTest, StructCreationAndAccess)
{
    eval(R"(
        s.name = 'test';
        s.value = 42;
        s.data = [1 2 3];
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("s.value;")), 42.0);
}

TEST_F(EngineAdvancedTest, NestedStruct)
{
    eval(R"(
        s.inner.x = 10;
        s.inner.y = 20;
        s.inner.nested.z = 30;
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("s.inner.x;")), 10.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("s.inner.y;")), 20.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("s.inner.nested.z;")), 30.0);
}

TEST_F(EngineAdvancedTest, StructModification)
{
    eval(R"(
        s.x = 1;
        s.y = 2;
        s.x = s.x + s.y;
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("s.x;")), 3.0);
}

TEST_F(EngineAdvancedTest, StructPassedToFunction)
{
    eval(R"(
        function r = get_sum(s)
            r = s.x + s.y;
        end
        p.x = 10;
        p.y = 20;
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("get_sum(p);")), 30.0);
}

TEST_F(EngineAdvancedTest, StructWithMatrixField)
{
    eval(R"(
        s.mat = [1 2; 3 4];
        x = s.mat(2, 1);
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 3.0);
}

// =============================================================================
// If/Elseif/Else
// =============================================================================

TEST_F(EngineAdvancedTest, IfElseifElseChain)
{
    eval(R"(
        function r = classify(x)
            if x > 0
                r = 1;
            elseif x < 0
                r = -1;
            else
                r = 0;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("classify(5);")), 1.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("classify(-3);")), -1.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("classify(0);")), 0.0);
}

TEST_F(EngineAdvancedTest, NestedIf)
{
    eval(R"(
        function r = nested_check(a, b)
            if a > 0
                if b > 0
                    r = 1;
                else
                    r = 2;
                end
            else
                if b > 0
                    r = 3;
                else
                    r = 4;
                end
            end
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("nested_check(1, 1);")), 1.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("nested_check(1, -1);")), 2.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("nested_check(-1, 1);")), 3.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("nested_check(-1, -1);")), 4.0);
}

// =============================================================================
// Switch/Case
// =============================================================================

TEST_F(EngineAdvancedTest, SwitchCaseBasic)
{
    eval(R"(
        function r = day_type(d)
            switch d
                case 1
                    r = 10;
                case 2
                    r = 20;
                case 3
                    r = 30;
                otherwise
                    r = -1;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("day_type(1);")), 10.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("day_type(2);")), 20.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("day_type(3);")), 30.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("day_type(99);")), -1.0);
}

TEST_F(EngineAdvancedTest, SwitchWithStringCases)
{
    eval(R"(
        function r = color_code(c)
            switch c
                case 'red'
                    r = 1;
                case 'green'
                    r = 2;
                case 'blue'
                    r = 3;
                otherwise
                    r = 0;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("color_code('red');")), 1.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("color_code('blue');")), 3.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("color_code('yellow');")), 0.0);
}

// =============================================================================
// Try/Catch
// =============================================================================

TEST_F(EngineAdvancedTest, TryCatchWithErrorMessage)
{
    eval(R"(
        try
            error('test error');
            result = 0;
        catch e
            result = 1;
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("result;")), 1.0);
}

TEST_F(EngineAdvancedTest, TryCatchNestedTry)
{
    eval(R"(
        result = 0;
        try
            try
                error('inner');
            catch
                result = result + 1;
                error('rethrow');
            end
        catch
            result = result + 10;
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("result;")), 11.0);
}

TEST_F(EngineAdvancedTest, TryCatchNoError)
{
    eval(R"(
        try
            result = 42;
        catch
            result = -1;
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("result;")), 42.0);
}

// =============================================================================
// Scoping
// =============================================================================

TEST_F(EngineAdvancedTest, FunctionLocalScope)
{
    eval("x = 100;");
    eval(R"(
        function r = local_test()
            x = 5;
            r = x;
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("local_test();")), 5.0);
    // Global x should be unchanged
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 100.0);
}

TEST_F(EngineAdvancedTest, NestedFunctionScopes)
{
    eval(R"(
        function r = outer(x)
            function r2 = inner(y)
                r2 = y * 2;
            end
            r = inner(x) + x;
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("outer(5);")), 15.0);
}

TEST_F(EngineAdvancedTest, GlobalVariable)
{
    eval(R"(
        global g_counter;
        g_counter = 0;
        function increment()
            global g_counter;
            g_counter = g_counter + 1;
        end
    )");
    eval("increment();");
    eval("increment();");
    eval("increment();");
    EXPECT_DOUBLE_EQ(toDouble(eval("g_counter;")), 3.0);
}

// =============================================================================
// Алгоритмы
// =============================================================================

TEST_F(EngineAdvancedTest, BubbleSort)
{
    eval(R"(
        function A = bubble_sort(A)
            n = length(A);
            for i = 1:n-1
                for j = 1:n-i
                    if A(j) > A(j+1)
                        temp = A(j);
                        A(j) = A(j+1);
                        A(j+1) = temp;
                    end
                end
            end
        end
    )");
    eval("sorted = bubble_sort([5 3 8 1 9 2 7 4 6]);");
    MValue sorted = eval("sorted;");
    for (size_t i = 0; i < 9; ++i) {
        expectElem(sorted, i, static_cast<double>(i + 1));
    }
}

TEST_F(EngineAdvancedTest, GCD_Euclidean)
{
    eval(R"(
        function r = gcd_func(a, b)
            while b ~= 0
                temp = b;
                b = mod(a, b);
                a = temp;
            end
            r = a;
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("gcd_func(48, 18);")), 6.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("gcd_func(100, 75);")), 25.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("gcd_func(7, 13);")), 1.0);
}

TEST_F(EngineAdvancedTest, MatrixPowerFibonacci)
{
    eval(R"(
        function R = mat_power(A, n)
            R = eye(size(A, 1));
            for i = 1:n
                R = R * A;
            end
        end
    )");
    eval("M = mat_power([1 1; 1 0], 10);");
    MValue M = eval("M;");
    // [[F(11), F(10)], [F(10), F(9)]]
    expectElem2D(M, 0, 0, 89); // F(11)
    expectElem2D(M, 0, 1, 55); // F(10)
    expectElem2D(M, 1, 0, 55); // F(10)
    expectElem2D(M, 1, 1, 34); // F(9)
}

TEST_F(EngineAdvancedTest, NewtonMethodSqrt)
{
    eval(R"(
        function r = my_sqrt(n)
            x = n;
            for i = 1:100
                x = (x + n/x) / 2;
            end
            r = x;
        end
    )");
    double result = toDouble(eval("my_sqrt(2);"));
    EXPECT_NEAR(result, std::sqrt(2.0), 1e-10);

    result = toDouble(eval("my_sqrt(144);"));
    EXPECT_NEAR(result, 12.0, 1e-10);
}

TEST_F(EngineAdvancedTest, PrimesSieve)
{
    eval(R"(
        function primes = sieve(n)
            is_prime = ones(1, n);
            is_prime(1) = 0;
            for i = 2:floor(sqrt(n))
                if is_prime(i)
                    for j = i*i:i:n
                        is_prime(j) = 0;
                    end
                end
            end
            primes = find(is_prime);
        end
    )");
    eval("p = sieve(30);");
    MValue p = eval("p;");
    // Primes up to 30: 2,3,5,7,11,13,17,19,23,29
    EXPECT_EQ(p.numel(), 10u);
    expectElem(p, 0, 2);
    expectElem(p, 9, 29);
}

TEST_F(EngineAdvancedTest, MatrixDeterminant2x2)
{
    eval(R"(
        function d = det2(A)
            d = A(1,1)*A(2,2) - A(1,2)*A(2,1);
        end
    )");
    eval("d = det2([3 8; 4 6]);");
    EXPECT_DOUBLE_EQ(toDouble(eval("d;")), -14.0); // 3*6 - 8*4
}

TEST_F(EngineAdvancedTest, RunningAverage)
{
    eval(R"(
        function avg = running_avg(data)
            n = length(data);
            avg = zeros(1, n);
            s = 0;
            for i = 1:n
                s = s + data(i);
                avg(i) = s / i;
            end
        end
    )");
    eval("a = running_avg([2 4 6 8 10]);");
    MValue a = eval("a;");
    expectElem(a, 0, 2.0);
    expectElem(a, 1, 3.0);
    expectElem(a, 2, 4.0);
    expectElem(a, 3, 5.0);
    expectElem(a, 4, 6.0);
}

TEST_F(EngineAdvancedTest, CollatzSequence)
{
    eval(R"(
        function steps = collatz(n)
            steps = 0;
            while n ~= 1
                if mod(n, 2) == 0
                    n = n / 2;
                else
                    n = 3*n + 1;
                end
                steps = steps + 1;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("collatz(1);")), 0.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("collatz(6);")), 8.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("collatz(27);")), 111.0);
}

// =============================================================================
// Строки
// =============================================================================

TEST_F(EngineAdvancedTest, StringConcatenation)
{
    eval(R"(
        a = 'Hello';
        b = ' World';
        c = [a, b];
    )");
    MValue c = eval("c;");
    EXPECT_TRUE(c.isChar());
    EXPECT_EQ(c.toString(), "Hello World");
}

TEST_F(EngineAdvancedTest, StringComparison)
{
    eval(R"(
        a = 'abc';
        b = 'abc';
        c = 'def';
        r1 = strcmp(a, b);
        r2 = strcmp(a, c);
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("r1;")), 1.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("r2;")), 0.0);
}

// =============================================================================
// Логические операции
// =============================================================================

TEST_F(EngineAdvancedTest, ShortCircuitAnd)
{
    eval(R"(
        x = 0;
        if false && (x = 1)
            y = 1;
        else
            y = 0;
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 0.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("y;")), 0.0);
}

TEST_F(EngineAdvancedTest, ShortCircuitOr)
{
    eval(R"(
        x = 0;
        if true || (x = 1)
            y = 1;
        else
            y = 0;
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 0.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("y;")), 1.0);
}

TEST_F(EngineAdvancedTest, LogicalOperatorsOnMatrices)
{
    eval(R"(
        A = [1 0 1; 0 1 0];
        B = [1 1 0; 0 0 1];
        C = A & B;
        D = A | B;
        E = ~A;
    )");
    MValue C = eval("C;");
    expectElem2D(C, 0, 0, 1);
    expectElem2D(C, 0, 1, 0);
    expectElem2D(C, 0, 2, 0);

    MValue D = eval("D;");
    expectElem2D(D, 0, 0, 1);
    expectElem2D(D, 0, 1, 1);
    expectElem2D(D, 0, 2, 1);

    MValue E = eval("E;");
    expectElem2D(E, 0, 0, 0);
    expectElem2D(E, 0, 1, 1);
}

// =============================================================================
// Comparison operators returning matrices
// =============================================================================

TEST_F(EngineAdvancedTest, ComparisonOnMatrices)
{
    eval(R"(
        A = [1 5 3 7 2];
        B = A > 3;
        C = A == 3;
        D = A <= 3;
    )");
    MValue B = eval("B;");
    expectElem(B, 0, 0);
    expectElem(B, 1, 1);
    expectElem(B, 2, 0);
    expectElem(B, 3, 1);
    expectElem(B, 4, 0);
}

// =============================================================================
// Edge cases and error handling
// =============================================================================

TEST_F(EngineAdvancedTest, EmptyMatrix)
{
    eval("A = [];");
    MValue A = eval("A;");
    EXPECT_EQ(A.numel(), 0u);
}

TEST_F(EngineAdvancedTest, ScalarMatrixOperations)
{
    eval(R"(
        A = [1 2 3];
        B = A + 10;
        C = 2 * A;
        D = A / 2;
    )");
    MValue B = eval("B;");
    expectElem(B, 0, 11);
    expectElem(B, 1, 12);
    expectElem(B, 2, 13);

    MValue C = eval("C;");
    expectElem(C, 0, 2);
    expectElem(C, 1, 4);
    expectElem(C, 2, 6);
}

TEST_F(EngineAdvancedTest, DimensionMismatchError)
{
    EXPECT_THROW(eval("[1 2 3] + [1 2];"), std::runtime_error);
}

TEST_F(EngineAdvancedTest, UndefinedVariableError)
{
    EXPECT_THROW(eval("x_undefined_var;"), std::runtime_error);
}

TEST_F(EngineAdvancedTest, UndefinedFunctionError)
{
    EXPECT_THROW(eval("nonexistent_function(1, 2);"), std::runtime_error);
}

TEST_F(EngineAdvancedTest, IndexOutOfBoundsError)
{
    eval("A = [1 2 3];");
    EXPECT_THROW(eval("A(10);"), std::runtime_error);
}

TEST_F(EngineAdvancedTest, WrongNumberOfArguments)
{
    eval(R"(
        function r = add2(a, b)
            r = a + b;
        end
    )");
    EXPECT_THROW(eval("add2(1, 2, 3);"), std::runtime_error);
}

// =============================================================================
// Complex expression evaluation
// =============================================================================

TEST_F(EngineAdvancedTest, ChainedOperations)
{
    eval("x = ((2 + 3) * 4 - 6) / 7;");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), (5.0 * 4.0 - 6.0) / 7.0);
}

TEST_F(EngineAdvancedTest, OperatorPrecedence)
{
    eval("x = 2 + 3 * 4;");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 14.0);

    eval("y = 2 * 3 + 4;");
    EXPECT_DOUBLE_EQ(toDouble(eval("y;")), 10.0);

    eval("z = 2 ^ 3 ^ 2;");
    // MATLAB: right-associative: 2^(3^2) = 2^9 = 512
    EXPECT_DOUBLE_EQ(toDouble(eval("z;")), 512.0);
}

TEST_F(EngineAdvancedTest, UnaryMinus)
{
    eval("x = -5;");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), -5.0);

    eval("y = -(-3);");
    EXPECT_DOUBLE_EQ(toDouble(eval("y;")), 3.0);

    eval("z = 2 * -3;");
    EXPECT_DOUBLE_EQ(toDouble(eval("z;")), -6.0);
}

TEST_F(EngineAdvancedTest, NegativeMatrixElements)
{
    eval("A = [-1 -2; -3 -4];");
    MValue A = eval("A;");
    expectElem2D(A, 0, 0, -1);
    expectElem2D(A, 1, 1, -4);
}

// =============================================================================
// Built-in functions
// =============================================================================

TEST_F(EngineAdvancedTest, SizeFunction)
{
    eval(R"(
        A = [1 2 3; 4 5 6];
        [r, c] = size(A);
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("r;")), 2.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("c;")), 3.0);
}

TEST_F(EngineAdvancedTest, LengthFunction)
{
    eval("A = [1 2 3 4 5];");
    EXPECT_DOUBLE_EQ(toDouble(eval("length(A);")), 5.0);

    eval("B = [1; 2; 3];");
    EXPECT_DOUBLE_EQ(toDouble(eval("length(B);")), 3.0);
}

TEST_F(EngineAdvancedTest, ZerosOnesEye)
{
    eval("Z = zeros(3, 4);");
    MValue Z = eval("Z;");
    EXPECT_EQ(rows(Z), 3u);
    EXPECT_EQ(cols(Z), 4u);
    expectElem2D(Z, 1, 2, 0);

    eval("O = ones(2, 3);");
    MValue O = eval("O;");
    expectElem2D(O, 1, 2, 1);

    eval("I = eye(3);");
    MValue I = eval("I;");
    expectElem2D(I, 0, 0, 1);
    expectElem2D(I, 1, 1, 1);
    expectElem2D(I, 0, 1, 0);
    expectElem2D(I, 2, 2, 1);
}

TEST_F(EngineAdvancedTest, SumMinMax)
{
    eval("A = [3 1 4 1 5 9 2 6];");
    EXPECT_DOUBLE_EQ(toDouble(eval("sum(A);")), 31.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("min(A);")), 1.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("max(A);")), 9.0);
}

TEST_F(EngineAdvancedTest, ReshapeFunction)
{
    eval(R"(
        A = [1 2 3 4 5 6];
        B = reshape(A, 2, 3);
    )");
    MValue B = eval("B;");
    EXPECT_EQ(rows(B), 2u);
    EXPECT_EQ(cols(B), 3u);
}

TEST_F(EngineAdvancedTest, LinspaceFunction)
{
    eval("x = linspace(0, 1, 5);");
    MValue x = eval("x;");
    EXPECT_EQ(x.numel(), 5u);
    expectElem(x, 0, 0.0);
    expectElem(x, 4, 1.0);
    EXPECT_NEAR(x(2), 0.5, 1e-10);
}

TEST_F(EngineAdvancedTest, AbsFunction)
{
    eval("x = abs([-3 -1 0 2 -5]);");
    MValue x = eval("x;");
    expectElem(x, 0, 3);
    expectElem(x, 1, 1);
    expectElem(x, 2, 0);
    expectElem(x, 4, 5);
}

TEST_F(EngineAdvancedTest, FloorCeilRound)
{
    eval(R"(
        a = floor(3.7);
        b = ceil(3.2);
        c = round(3.5);
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("a;")), 3.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("b;")), 4.0);
    // MATLAB rounds 3.5 to 4
    EXPECT_DOUBLE_EQ(toDouble(eval("c;")), 4.0);
}

TEST_F(EngineAdvancedTest, ModFunction)
{
    EXPECT_DOUBLE_EQ(toDouble(eval("mod(10, 3);")), 1.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("mod(10, 5);")), 0.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("mod(7, 2);")), 1.0);
}

TEST_F(EngineAdvancedTest, NumelFunction)
{
    eval("A = ones(3, 4);");
    EXPECT_DOUBLE_EQ(toDouble(eval("numel(A);")), 12.0);
}

// =============================================================================
// Return from function
// =============================================================================

TEST_F(EngineAdvancedTest, EarlyReturn)
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
    EXPECT_DOUBLE_EQ(toDouble(eval("early_ret(-5);")), -1.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("early_ret(3);")), 6.0);
}

TEST_F(EngineAdvancedTest, ReturnInLoop)
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
    EXPECT_DOUBLE_EQ(toDouble(eval("find_first([5 3 8 1 9], 8);")), 3.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("find_first([5 3 8 1 9], 7);")), -1.0);
}

// =============================================================================
// Complex control flow combinations
// =============================================================================

TEST_F(EngineAdvancedTest, BreakContinueInteraction)
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
    MValue result = eval("result;");
    // Odd numbers <= 7: 1, 3, 5, 7
    EXPECT_EQ(result.numel(), 4u);
    expectElem(result, 0, 1);
    expectElem(result, 1, 3);
    expectElem(result, 2, 5);
    expectElem(result, 3, 7);
}

TEST_F(EngineAdvancedTest, ForLoopWithFunctionCalls)
{
    eval(R"(
        function r = square(x)
            r = x * x;
        end
        total = 0;
        for i = 1:5
            total = total + square(i);
        end
    )");
    // 1 + 4 + 9 + 16 + 25 = 55
    EXPECT_DOUBLE_EQ(toDouble(eval("total;")), 55.0);
}

// =============================================================================
// Display behavior
// =============================================================================

TEST_F(EngineAdvancedTest, SemicolonSuppressesOutput)
{
    captured_output.clear();
    eval("x = 42;");
    EXPECT_TRUE(captured_output.empty());
}

TEST_F(EngineAdvancedTest, NoSemicolonShowsOutput)
{
    captured_output.clear();
    eval("x = 42");
    EXPECT_FALSE(captured_output.empty());
    EXPECT_TRUE(captured_output.find("42") != std::string::npos);
}

// =============================================================================
// Numeric edge cases
// =============================================================================

TEST_F(EngineAdvancedTest, InfinityHandling)
{
    eval("x = 1/0;");
    EXPECT_TRUE(std::isinf(toDouble(eval("x;"))));

    eval("y = -1/0;");
    double yval = toDouble(eval("y;"));
    EXPECT_TRUE(std::isinf(yval));
    EXPECT_TRUE(yval < 0);
}

TEST_F(EngineAdvancedTest, NaNHandling)
{
    eval("x = 0/0;");
    EXPECT_TRUE(std::isnan(toDouble(eval("x;"))));
}

TEST_F(EngineAdvancedTest, LargeNumbers)
{
    eval("x = 1e15 + 1;");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 1e15 + 1);
}

// =============================================================================
// Dynamic programming
// =============================================================================

TEST_F(EngineAdvancedTest, KnapsackDP)
{
    eval(R"(
        function maxval = knapsack(W, weights, values, n)
            dp = zeros(n+1, W+1);
            for i = 1:n
                for w = 0:W
                    if weights(i) <= w
                        dp(i+1, w+1) = max(dp(i, w+1), dp(i, w+1-weights(i)) + values(i));
                    else
                        dp(i+1, w+1) = dp(i, w+1);
                    end
                end
            end
            maxval = dp(n+1, W+1);
        end
    )");
    eval("result = knapsack(50, [10 20 30], [60 100 120], 3);");
    EXPECT_DOUBLE_EQ(toDouble(eval("result;")), 220.0);
}

TEST_F(EngineAdvancedTest, PascalTriangle)
{
    eval(R"(
        function C = pascal_triangle(n)
            C = zeros(n, n);
            for i = 1:n
                C(i, 1) = 1;
                for j = 2:i
                    C(i, j) = C(i-1, j-1) + C(i-1, j);
                end
            end
        end
    )");
    eval("P = pascal_triangle(6);");
    MValue P = eval("P;");
    // Row 6 (0-indexed row 5): 1 5 10 10 5 1
    expectElem2D(P, 5, 0, 1);
    expectElem2D(P, 5, 1, 5);
    expectElem2D(P, 5, 2, 10);
    expectElem2D(P, 5, 3, 10);
    expectElem2D(P, 5, 4, 5);
    expectElem2D(P, 5, 5, 1);
}

TEST_F(EngineAdvancedTest, MatrixTraceManual)
{
    eval(R"(
        function t = my_trace(A)
            [n, m] = size(A);
            t = 0;
            for i = 1:min(n, m)
                t = t + A(i, i);
            end
        end
    )");
    eval("t = my_trace([1 2 3; 4 5 6; 7 8 9]);");
    EXPECT_DOUBLE_EQ(toDouble(eval("t;")), 15.0); // 1+5+9
}

TEST_F(EngineAdvancedTest, MatrixIsSymmetric)
{
    eval(R"(
        function r = is_symmetric(A)
            [n, m] = size(A);
            if n ~= m
                r = 0;
                return;
            end
            r = 1;
            for i = 1:n
                for j = 1:n
                    if A(i,j) ~= A(j,i)
                        r = 0;
                        return;
                    end
                end
            end
        end
    )");
    eval("r1 = is_symmetric([1 2 3; 2 5 6; 3 6 9]);");
    EXPECT_DOUBLE_EQ(toDouble(eval("r1;")), 1.0);

    eval("r2 = is_symmetric([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(toDouble(eval("r2;")), 0.0);
}

// =============================================================================
// Function redefinition
// =============================================================================

TEST_F(EngineAdvancedTest, FunctionRedefinition)
{
    eval(R"(
        function r = f(x)
            r = x + 1;
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("f(5);")), 6.0);

    eval(R"(
        function r = f(x)
            r = x * 2;
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("f(5);")), 10.0);
}

// =============================================================================
// Numerical methods
// =============================================================================

TEST_F(EngineAdvancedTest, NumericalIntegrationTrapezoidal)
{
    eval(R"(
        function area = trapz_integrate(a, b, n)
            h = (b - a) / n;
            s = 0;
            for i = 1:n-1
                x = a + i * h;
                s = s + x^2;
            end
            area = h * (a^2/2 + s + b^2/2);
        end
    )");
    double result = toDouble(eval("trapz_integrate(0, 1, 1000);"));
    EXPECT_NEAR(result, 1.0 / 3.0, 1e-5);
}

TEST_F(EngineAdvancedTest, BisectionMethod)
{
    eval(R"(
        function root = bisection(a, b, tol)
            for i = 1:100
                mid = (a + b) / 2;
                fa = a^2 - 2;
                fmid = mid^2 - 2;
                if abs(fmid) < tol
                    root = mid;
                    return;
                end
                if fa * fmid < 0
                    b = mid;
                else
                    a = mid;
                end
            end
            root = (a + b) / 2;
        end
    )");
    double root = toDouble(eval("bisection(1, 2, 1e-10);"));
    EXPECT_NEAR(root, std::sqrt(2.0), 1e-9);
}

TEST_F(EngineAdvancedTest, TaylorSeriesSin)
{
    eval(R"(
        function s = taylor_sin(x, n)
            s = 0;
            for k = 0:n
                term = ((-1)^k) * (x^(2*k+1));
                fact = 1;
                for j = 1:(2*k+1)
                    fact = fact * j;
                end
                s = s + term / fact;
            end
        end
    )");
    double result = toDouble(eval("taylor_sin(1.0, 10);"));
    EXPECT_NEAR(result, std::sin(1.0), 1e-10);
}

TEST_F(EngineAdvancedTest, TaylorSeriesExp)
{
    eval(R"(
        function s = taylor_exp(x, n)
            s = 0;
            fact = 1;
            for k = 0:n
                if k > 0
                    fact = fact * k;
                end
                s = s + x^k / fact;
            end
        end
    )");
    double result = toDouble(eval("taylor_exp(1.0, 20);"));
    EXPECT_NEAR(result, std::exp(1.0), 1e-10);
}

TEST_F(EngineAdvancedTest, PowerIterationDominantEigenvalue)
{
    eval(R"(
        function lambda = power_iter(A, niter)
            [n, m] = size(A);
            x = ones(n, 1);
            for i = 1:niter
                y = A * x;
                lambda = max(abs(y));
                x = y / lambda;
            end
        end
    )");
    eval("ev = power_iter([2 1; 1 2], 50);");
    double ev = toDouble(eval("ev;"));
    EXPECT_NEAR(ev, 3.0, 1e-6);
}

TEST_F(EngineAdvancedTest, GaussianElimination)
{
    eval(R"(
        function x = gauss_solve(A, b)
            n = length(b);
            Ab = [A, b];
            for k = 1:n-1
                for i = k+1:n
                    factor = Ab(i, k) / Ab(k, k);
                    for j = k:n+1
                        Ab(i, j) = Ab(i, j) - factor * Ab(k, j);
                    end
                end
            end
            x = zeros(n, 1);
            for i = n:-1:1
                s = 0;
                for j = i+1:n
                    s = s + Ab(i, j) * x(j);
                end
                x(i) = (Ab(i, n+1) - s) / Ab(i, i);
            end
        end
    )");
    // Solve: 2x + y = 5, x + 3y = 7 => x=1.6, y=1.8
    eval("x = gauss_solve([2 1; 1 3], [5; 7]);");
    MValue x = eval("x;");
    EXPECT_NEAR(x(0), 1.6, 1e-10);
    EXPECT_NEAR(x(1), 1.8, 1e-10);
}

// =============================================================================
// Complex data structure manipulation
// =============================================================================

TEST_F(EngineAdvancedTest, StructArrayOfResults)
{
    eval(R"(
        results.iterations = 0;
        results.values = [];
        x = 1;
        for i = 1:5
            x = x * 2;
            results.iterations = results.iterations + 1;
            results.values = [results.values, x];
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("results.iterations;")), 5.0);
    MValue values = eval("results.values;");
    EXPECT_EQ(values.numel(), 5u);
    expectElem(values, 0, 2);
    expectElem(values, 1, 4);
    expectElem(values, 4, 32);
}

TEST_F(EngineAdvancedTest, CellArrayAsStack)
{
    eval(R"(
        stack = {};
        stack_size = 0;

        function [s, n] = push(s, n, val)
            n = n + 1;
            s{n} = val;
        end

        function [s, n, val] = pop(s, n)
            val = s{n};
            n = n - 1;
        end

        [stack, stack_size] = push(stack, stack_size, 10);
        [stack, stack_size] = push(stack, stack_size, 20);
        [stack, stack_size] = push(stack, stack_size, 30);
        [stack, stack_size, top] = pop(stack, stack_size);
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("top;")), 30.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("stack_size;")), 2.0);
}

// =============================================================================
// Stress tests
// =============================================================================

TEST_F(EngineAdvancedTest, LargeLoopIteration)
{
    eval(R"(
        s = 0;
        for i = 1:10000
            s = s + i;
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("s;")), 50005000.0);
}

TEST_F(EngineAdvancedTest, LargeMatrixCreation)
{
    eval(R"(
        A = zeros(100, 100);
        for i = 1:100
            for j = 1:100
                A(i, j) = i + j;
            end
        end
        total = sum(sum(A));
    )");
    // sum of (i+j) for i=1..100, j=1..100
    // = 100*sum(1..100) + 100*sum(1..100) = 100*5050 + 100*5050 = 1010000
    EXPECT_DOUBLE_EQ(toDouble(eval("total;")), 1010000.0);
}

TEST_F(EngineAdvancedTest, ManyFunctionCalls)
{
    eval(R"(
        function r = increment(x)
            r = x + 1;
        end
        val = 0;
        for i = 1:1000
            val = increment(val);
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("val;")), 1000.0);
}

// =============================================================================
// End keyword in indexing
// =============================================================================

TEST_F(EngineAdvancedTest, EndKeywordInIndexing)
{
    eval(R"(
        A = [10 20 30 40 50];
        x = A(end);
        y = A(end-1);
        z = A(end-2:end);
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 50.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("y;")), 40.0);

    MValue z = eval("z;");
    EXPECT_EQ(z.numel(), 3u);
    expectElem(z, 0, 30);
    expectElem(z, 1, 40);
    expectElem(z, 2, 50);
}

TEST_F(EngineAdvancedTest, EndKeywordIn2DIndexing)
{
    eval(R"(
        A = [1 2 3; 4 5 6; 7 8 9];
        x = A(end, end);
        y = A(1:end, end);
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("x;")), 9.0);

    MValue y = eval("y;");
    EXPECT_EQ(y.numel(), 3u);
    expectElem(y, 0, 3);
    expectElem(y, 1, 6);
    expectElem(y, 2, 9);
}

// =============================================================================
// Function call chains & higher-order
// =============================================================================

TEST_F(EngineAdvancedTest, FunctionCallingFunction)
{
    eval(R"(
        function r = double_val(x)
            r = x * 2;
        end

        function r = quad_val(x)
            r = double_val(double_val(x));
        end
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("quad_val(3);")), 12.0);
}

TEST_F(EngineAdvancedTest, FunctionAsArgument)
{
    eval(R"(
        function r = apply_twice(f, x)
            r = f(f(x));
        end
        add3 = @(x) x + 3;
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("apply_twice(add3, 10);")), 16.0);
}

TEST_F(EngineAdvancedTest, MapFunction)
{
    eval(R"(
        function result = my_map(f, A)
            n = length(A);
            result = zeros(1, n);
            for i = 1:n
                result(i) = f(A(i));
            end
        end
    )");
    eval("r = my_map(@(x) x^2, [1 2 3 4 5]);");
    MValue r = eval("r;");
    expectElem(r, 0, 1);
    expectElem(r, 1, 4);
    expectElem(r, 2, 9);
    expectElem(r, 3, 16);
    expectElem(r, 4, 25);
}

TEST_F(EngineAdvancedTest, ReduceFunction)
{
    eval(R"(
        function result = my_reduce(f, A, init)
            result = init;
            for i = 1:length(A)
                result = f(result, A(i));
            end
        end
    )");
    eval("s = my_reduce(@(a, b) a + b, [1 2 3 4 5], 0);");
    EXPECT_DOUBLE_EQ(toDouble(eval("s;")), 15.0);

    eval("p = my_reduce(@(a, b) a * b, [1 2 3 4 5], 1);");
    EXPECT_DOUBLE_EQ(toDouble(eval("p;")), 120.0);
}

// =============================================================================
// Misc complex scenarios
// =============================================================================

TEST_F(EngineAdvancedTest, VariableReassignmentTypes)
{
    eval(R"(
        x = 42;
        x = 'hello';
        x = [1 2 3];
        x = {1, 'two', 3};
    )");
    MValue x = eval("x;");
    EXPECT_TRUE(x.isCell());
}

TEST_F(EngineAdvancedTest, SelfReferentialUpdate)
{
    eval(R"(
        A = [1 2 3 4 5];
        for i = 2:length(A)
            A(i) = A(i) + A(i-1);
        end
    )");
    // Prefix sums: [1, 3, 6, 10, 15]
    MValue A = eval("A;");
    expectElem(A, 0, 1);
    expectElem(A, 1, 3);
    expectElem(A, 2, 6);
    expectElem(A, 3, 10);
    expectElem(A, 4, 15);
}

TEST_F(EngineAdvancedTest, ConvolutionManual)
{
    eval(R"(
        function c = my_conv(a, b)
            na = length(a);
            nb = length(b);
            nc = na + nb - 1;
            c = zeros(1, nc);
            for i = 1:na
                for j = 1:nb
                    c(i+j-1) = c(i+j-1) + a(i) * b(j);
                end
            end
        end
    )");
    eval("r = my_conv([1 2 3], [4 5]);");
    MValue r = eval("r;");
    // [1*4, 1*5+2*4, 2*5+3*4, 3*5] = [4, 13, 22, 15]
    EXPECT_EQ(r.numel(), 4u);
    expectElem(r, 0, 4);
    expectElem(r, 1, 13);
    expectElem(r, 2, 22);
    expectElem(r, 3, 15);
}

TEST_F(EngineAdvancedTest, RecursiveQuicksort)
{
    eval(R"(
        function A = qsort(A)
            n = length(A);
            if n <= 1
                return;
            end
            pivot = A(floor(n/2) + 1);
            left = [];
            middle = [];
            right = [];
            for i = 1:n
                if A(i) < pivot
                    left = [left, A(i)];
                elseif A(i) == pivot
                    middle = [middle, A(i)];
                else
                    right = [right, A(i)];
                end
            end
            A = [qsort(left), middle, qsort(right)];
        end
    )");
    eval("sorted = qsort([3 6 8 10 1 2 1]);");
    MValue sorted = eval("sorted;");
    EXPECT_EQ(sorted.numel(), 7u);
    expectElem(sorted, 0, 1);
    expectElem(sorted, 1, 1);
    expectElem(sorted, 2, 2);
    expectElem(sorted, 3, 3);
    expectElem(sorted, 4, 6);
    expectElem(sorted, 5, 8);
    expectElem(sorted, 6, 10);
}

TEST_F(EngineAdvancedTest, MergeSort)
{
    eval(R"(
        function result = merge(a, b)
            result = [];
            i = 1;
            j = 1;
            while i <= length(a) && j <= length(b)
                if a(i) <= b(j)
                    result = [result, a(i)];
                    i = i + 1;
                else
                    result = [result, b(j)];
                    j = j + 1;
                end
            end
            while i <= length(a)
                result = [result, a(i)];
                i = i + 1;
            end
            while j <= length(b)
                result = [result, b(j)];
                j = j + 1;
            end
        end

        function A = msort(A)
            n = length(A);
            if n <= 1
                return;
            end
            mid = floor(n / 2);
            left = msort(A(1:mid));
            right = msort(A(mid+1:end));
            A = merge(left, right);
        end
    )");
    eval("sorted = msort([38 27 43 3 9 82 10]);");
    MValue sorted = eval("sorted;");
    EXPECT_EQ(sorted.numel(), 7u);
    expectElem(sorted, 0, 3);
    expectElem(sorted, 1, 9);
    expectElem(sorted, 2, 10);
    expectElem(sorted, 3, 27);
    expectElem(sorted, 4, 38);
    expectElem(sorted, 5, 43);
    expectElem(sorted, 6, 82);
}

// =============================================================================
// Anonymous functions — advanced
// =============================================================================

TEST_F(EngineAdvancedTest, AnonymousFunctionInCellArray)
{
    eval(R"(
        ops = {@(x,y) x+y, @(x,y) x-y, @(x,y) x*y, @(x,y) x/y};
        r1 = ops{1}(10, 3);
        r2 = ops{2}(10, 3);
        r3 = ops{3}(10, 3);
        r4 = ops{4}(10, 3);
    )");
    EXPECT_DOUBLE_EQ(toDouble(eval("r1;")), 13.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("r2;")), 7.0);
    EXPECT_DOUBLE_EQ(toDouble(eval("r3;")), 30.0);
    EXPECT_NEAR(toDouble(eval("r4;")), 10.0 / 3.0, 1e-10);
}

TEST_F(EngineAdvancedTest, CounterWithClosure)
{
    eval(R"(
        function [inc, get] = make_counter()
            count = 0;
            inc = @() count + 1;
            get = @() count;
        end
    )");
    eval("[inc, get] = make_counter();");
    MValue r = eval("get();");
    EXPECT_DOUBLE_EQ(toDouble(r), 0.0);
}

// =============================================================================
// Matrix operations — assignment patterns
// =============================================================================

TEST_F(EngineAdvancedTest, MatrixRowAssignment)
{
    eval(R"(
        A = zeros(3, 3);
        A(2, :) = [4 5 6];
    )");
    MValue A = eval("A;");
    expectElem2D(A, 1, 0, 4);
    expectElem2D(A, 1, 1, 5);
    expectElem2D(A, 1, 2, 6);
    expectElem2D(A, 0, 0, 0);
}

TEST_F(EngineAdvancedTest, MatrixColumnAssignment)
{
    eval(R"(
        A = zeros(3, 3);
        A(:, 2) = [10; 20; 30];
    )");
    MValue A = eval("A;");
    expectElem2D(A, 0, 1, 10);
    expectElem2D(A, 1, 1, 20);
    expectElem2D(A, 2, 1, 30);
}

TEST_F(EngineAdvancedTest, SubmatrixAssignment)
{
    eval(R"(
        A = zeros(4, 4);
        A(2:3, 2:3) = [5 6; 7 8];
    )");
    MValue A = eval("A;");
    expectElem2D(A, 1, 1, 5);
    expectElem2D(A, 1, 2, 6);
    expectElem2D(A, 2, 1, 7);
    expectElem2D(A, 2, 2, 8);
    expectElem2D(A, 0, 0, 0);
    expectElem2D(A, 3, 3, 0);
}

TEST_F(EngineAdvancedTest, DiagonalExtraction)
{
    eval(R"(
        function d = my_diag(A)
            [n, m] = size(A);
            k = min(n, m);
            d = zeros(k, 1);
            for i = 1:k
                d(i) = A(i, i);
            end
        end
        d = my_diag([1 2 3; 4 5 6; 7 8 9]);
    )");
    MValue d = eval("d;");
    expectElem(d, 0, 1);
    expectElem(d, 1, 5);
    expectElem(d, 2, 9);
}

// =============================================================================
// Type coercion
// =============================================================================

TEST_F(EngineAdvancedTest, TypeCoercionBroadcast)
{
    eval("r = [1 2 3] + 10;");
    MValue r = eval("r;");
    expectElem(r, 0, 11);
    expectElem(r, 1, 12);
    expectElem(r, 2, 13);
}
