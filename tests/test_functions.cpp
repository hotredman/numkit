// tests/test_functions.cpp — Functions, handles, closures, recursion
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

class FunctionTest : public DualEngineTest {};

TEST_P(FunctionTest, SimpleFunction)
{
    eval("function y = square(x)\n  y = x^2;\nend");
    eval("r = square(7);");
    EXPECT_DOUBLE_EQ(getVar("r"), 49.0);
}

TEST_P(FunctionTest, FunctionMultiReturn)
{
    eval("function [mn, mx] = minmax(v)\n  mn = min(v);\n  mx = max(v);\nend");
    eval("[a, b] = minmax([3 1 4 1 5]);");
    EXPECT_DOUBLE_EQ(getVar("a"), 1.0);
    EXPECT_DOUBLE_EQ(getVar("b"), 5.0);
}

TEST_P(FunctionTest, RecursiveFunction)
{
    eval("function n = fact(x)\n  if x <= 1, n = 1; else, n = x * fact(x-1); end\nend");
    eval("r = fact(6);");
    EXPECT_DOUBLE_EQ(getVar("r"), 720.0);
}

TEST_P(FunctionTest, AnonFunction)
{
    eval("f = @(x, y) x + y;");
    eval("r = f(3, 4);");
    EXPECT_DOUBLE_EQ(getVar("r"), 7.0);
}

TEST_P(FunctionTest, FunctionHandle)
{
    eval("f = @sin; r = f(pi/2);");
    EXPECT_NEAR(getVar("r"), 1.0, 1e-12);
}

TEST_P(FunctionTest, NestedFunctionCalls)
{
    eval(R"(
        function y = add1(x)
            y = x + 1;
        end
        function y = double_it(x)
            y = x * 2;
        end
    )");
    eval("r = add1(double_it(5));");
    EXPECT_DOUBLE_EQ(getVar("r"), 11.0);
}

TEST_P(FunctionTest, FunctionWithDefaultValues)
{
    eval(R"(
        function y = myfun(x)
            if nargin < 1
                x = 10;
            end
            y = x * 2;
        end
    )");
    eval("r = myfun();");
    EXPECT_DOUBLE_EQ(getVar("r"), 20.0);
    eval("r2 = myfun(3);");
    EXPECT_DOUBLE_EQ(getVar("r2"), 6.0);
}

TEST_P(FunctionTest, AnonFuncCapturesValues)
{
    // Closure captures value at definition time
    eval("a = 10; f = @(x) x + a;");
    eval("a = 999;"); // changing a after doesn't affect closure
    eval("r = f(5);");
    EXPECT_DOUBLE_EQ(getVar("r"), 15.0);
}

TEST_P(FunctionTest, RecursiveFibonacci)
{
    eval(R"(
        function y = fib(n)
            if n <= 1
                y = n;
            else
                y = fib(n-1) + fib(n-2);
            end
        end
    )");
    eval("r = fib(10);");
    EXPECT_DOUBLE_EQ(getVar("r"), 55.0);
}

TEST_P(FunctionTest, MutualRecursion)
{
    eval(R"(
        function y = is_even(n)
            if n == 0, y = 1;
            else, y = is_odd(n - 1); end
        end
        function y = is_odd(n)
            if n == 0, y = 0;
            else, y = is_even(n - 1); end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("is_even(4);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("is_odd(3);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("is_even(3);"), 0.0);
}

INSTANTIATE_DUAL(FunctionTest);
