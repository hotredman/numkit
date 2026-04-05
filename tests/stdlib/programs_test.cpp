// tests/test_programs.cpp — Complex programs: algorithms, realistic scripts
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

class ProgramTest : public DualEngineTest
{};

TEST_P(ProgramTest, Fibonacci)
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

TEST_P(ProgramTest, BubbleSort)
{
    eval(R"(
        function v = bsort(v)
            n = length(v);
            for i = 1:n-1
                for j = 1:n-i
                    if v(j) > v(j+1)
                        tmp = v(j);
                        v(j) = v(j+1);
                        v(j+1) = tmp;
                    end
                end
            end
        end
    )");
    eval("r = bsort([5 3 1 4 2]);");
    auto *r = getVarPtr("r");
    ASSERT_EQ(r->numel(), 5u);
    for (size_t i = 0; i < 5; ++i)
        EXPECT_DOUBLE_EQ(r->doubleData()[i], static_cast<double>(i + 1));
}

TEST_P(ProgramTest, MatrixOps)
{
    eval(R"(
        A = [1 2; 3 4];
        B = A';
        C = A * B;
        d = sum(sum(C));
    )");
    // A*A' = [1 2;3 4]*[1 3;2 4] = [5 11;11 25], sum = 52
    EXPECT_DOUBLE_EQ(getVar("d"), 52.0);
}

TEST_P(ProgramTest, LogicalIndexing)
{
    eval("v = [1 2 3 4 5 6]; r = v(v > 3);");
    auto *r = getVarPtr("r");
    ASSERT_EQ(r->numel(), 3u);
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[1], 5.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[2], 6.0);
}

TEST_P(ProgramTest, GCD)
{
    eval(R"(
        function g = mygcd(a, b)
            while b ~= 0
                t = b;
                b = mod(a, b);
                a = t;
            end
            g = a;
        end
    )");
    eval("r = mygcd(48, 18);");
    EXPECT_DOUBLE_EQ(getVar("r"), 6.0);
}

TEST_P(ProgramTest, SelectionSort)
{
    eval(R"(
        function v = ssort(v)
            n = length(v);
            for i = 1:n-1
                mi = i;
                for j = i+1:n
                    if v(j) < v(mi)
                        mi = j;
                    end
                end
                if mi ~= i
                    tmp = v(i);
                    v(i) = v(mi);
                    v(mi) = tmp;
                end
            end
        end
    )");
    eval("r = ssort([9 1 5 3 7 2 8 4 6]);");
    auto *r = getVarPtr("r");
    ASSERT_EQ(r->numel(), 9u);
    for (size_t i = 0; i < 9; ++i)
        EXPECT_DOUBLE_EQ(r->doubleData()[i], static_cast<double>(i + 1));
}

TEST_P(ProgramTest, IsPrime)
{
    eval(R"(
        function r = isprime_m(n)
            if n < 2, r = 0; return; end
            r = 1;
            for i = 2:floor(sqrt(n))
                if mod(n, i) == 0
                    r = 0;
                    return;
                end
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("isprime_m(2);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isprime_m(7);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isprime_m(4);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isprime_m(1);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isprime_m(97);"), 1.0);
}

TEST_P(ProgramTest, PowerFunction)
{
    eval(R"(
        function r = mypow(base, exp)
            r = 1;
            for i = 1:exp
                r = r * base;
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("mypow(2, 10);"), 1024.0);
    EXPECT_DOUBLE_EQ(evalScalar("mypow(3, 4);"), 81.0);
}

TEST_P(ProgramTest, SumOfDigits)
{
    eval(R"(
        function s = sumdigits(n)
            s = 0;
            while n > 0
                s = s + mod(n, 10);
                n = floor(n / 10);
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("sumdigits(12345);"), 15.0);
    EXPECT_DOUBLE_EQ(evalScalar("sumdigits(9999);"), 36.0);
}

TEST_P(ProgramTest, MatrixTrace)
{
    eval(R"(
        function t = mytrace(A)
            t = 0;
            n = size(A, 1);
            for i = 1:n
                t = t + A(i, i);
            end
        end
    )");
    eval("r = mytrace([1 2 3; 4 5 6; 7 8 9]);");
    EXPECT_DOUBLE_EQ(getVar("r"), 15.0); // 1+5+9
}

TEST_P(ProgramTest, DotProduct)
{
    eval(R"(
        function d = mydot(a, b)
            d = 0;
            for i = 1:length(a)
                d = d + a(i) * b(i);
            end
        end
    )");
    eval("r = mydot([1 2 3], [4 5 6]);");
    EXPECT_DOUBLE_EQ(getVar("r"), 32.0); // 4+10+18
}

TEST_P(ProgramTest, NewtonSqrt)
{
    eval(R"(
        function r = mysqrt(x)
            r = x;
            for i = 1:50
                r = (r + x/r) / 2;
            end
        end
    )");
    EXPECT_NEAR(evalScalar("mysqrt(2);"), std::sqrt(2.0), 1e-10);
    EXPECT_NEAR(evalScalar("mysqrt(9);"), 3.0, 1e-10);
}

TEST_P(ProgramTest, CountPrimes)
{
    eval(R"(
        function r = isprime_m(n)
            if n < 2, r = 0; return; end
            r = 1;
            for i = 2:floor(sqrt(n))
                if mod(n, i) == 0
                    r = 0;
                    return;
                end
            end
        end
        function c = countprimes(n)
            c = 0;
            for i = 2:n
                c = c + isprime_m(i);
            end
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("countprimes(20);"), 8.0); // 2,3,5,7,11,13,17,19
}

TEST_P(ProgramTest, MatrixTransposeManual)
{
    eval(R"(
        function B = mytranspose(A)
            m = size(A, 1);
            n = size(A, 2);
            B = zeros(n, m);
            for i = 1:m
                for j = 1:n
                    B(j, i) = A(i, j);
                end
            end
        end
    )");
    eval("B = mytranspose([1 2 3; 4 5 6]);");
    auto *B = getVarPtr("B");
    EXPECT_EQ(rows(*B), 3u);
    EXPECT_EQ(cols(*B), 2u);
    EXPECT_DOUBLE_EQ((*B)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*B)(0, 1), 4.0);
    EXPECT_DOUBLE_EQ((*B)(2, 1), 6.0);
}

TEST_P(ProgramTest, CumulativeSum)
{
    eval(R"(
        function c = mycumsum(v)
            n = length(v);
            c = zeros(1, n);
            c(1) = v(1);
            for i = 2:n
                c(i) = c(i-1) + v(i);
            end
        end
    )");
    eval("r = mycumsum([1 2 3 4 5]);");
    auto *r = getVarPtr("r");
    ASSERT_EQ(r->numel(), 5u);
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[2], 6.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[3], 10.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[4], 15.0);
}

TEST_P(ProgramTest, ReverseArray)
{
    eval(R"(
        function v = myrev(v)
            n = length(v);
            for i = 1:floor(n/2)
                tmp = v(i);
                v(i) = v(n - i + 1);
                v(n - i + 1) = tmp;
            end
        end
    )");
    eval("r = myrev([1 2 3 4 5]);");
    auto *r = getVarPtr("r");
    ASSERT_EQ(r->numel(), 5u);
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 5.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[4], 1.0);
}

TEST_P(ProgramTest, TaylorSinApprox)
{
    eval(R"(
        function s = mysin(x)
            s = 0;
            term = x;
            for k = 1:10
                s = s + term;
                term = -term * x^2 / ((2*k)*(2*k+1));
            end
        end
    )");
    EXPECT_NEAR(evalScalar("mysin(pi/4);"), std::sin(M_PI / 4), 1e-10);
}

INSTANTIATE_DUAL(ProgramTest);