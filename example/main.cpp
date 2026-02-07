#include "MLabEngine.hpp"
#include "MLabStdLibrary.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

struct TestStats
{
    int total = 0;
    int passed = 0;
    int failed = 0;
};

static void printSection(const std::string &name)
{
    std::cout << "\n--- " << name << " ";
    int remaining = 50 - static_cast<int>(name.size());
    for (int i = 0; i < std::max(remaining, 0); ++i)
        std::cout << '-';
    std::cout << "\n";
}

static void runTest(mlab::Engine &engine,
                    const std::string &code,
                    const std::string &expected,
                    TestStats &stats)
{
    stats.total++;

    // перехватываем stdout движка в строку
    std::ostringstream capture;
    auto *oldBuf = std::cout.rdbuf(capture.rdbuf());

    bool ok = true;
    std::string error;
    try {
        engine.eval(code);
    } catch (const std::exception &e) {
        ok = false;
        error = e.what();
    }

    std::cout.rdbuf(oldBuf);

    // убираем trailing whitespace из захваченного вывода
    std::string actual = capture.str();
    while (!actual.empty() && (actual.back() == '\n' || actual.back() == ' '))
        actual.pop_back();

    std::cout << "\n";
    std::cout << "  Input:    " << code << "\n";
    std::cout << "  Expected: " << expected << "\n";

    if (!ok) {
        std::cout << "  Actual:   [ERROR] " << error << "\n";
        std::cout << "  Status:   FAIL\n";
        stats.failed++;
    } else {
        std::cout << "  Actual:   " << actual << "\n";
        if (actual == expected) {
            std::cout << "  Status:   OK\n";
            stats.passed++;
        } else {
            std::cout << "  Status:   MISMATCH\n";
            stats.failed++;
        }
    }
}

// для многострочного кода — без автоматической проверки,
// просто показываем что произошло
static void runSetup(mlab::Engine &engine,
                     const std::string &code,
                     const std::string &description,
                     TestStats &stats)
{
    stats.total++;

    std::cout << "\n";
    std::cout << "  Input:    " << description << "\n";

    try {
        engine.eval(code);
        std::cout << "  Status:   OK\n";
        stats.passed++;
    } catch (const std::exception &e) {
        std::cout << "  Status:   FAIL (" << e.what() << ")\n";
        stats.failed++;
    }
}

// для многострочного кода с ожидаемым выводом
static void runTestBlock(mlab::Engine &engine,
                         const std::string &code,
                         const std::string &description,
                         const std::string &expected,
                         TestStats &stats)
{
    stats.total++;

    std::ostringstream capture;
    auto *oldBuf = std::cout.rdbuf(capture.rdbuf());

    bool ok = true;
    std::string error;
    try {
        engine.eval(code);
    } catch (const std::exception &e) {
        ok = false;
        error = e.what();
    }

    std::cout.rdbuf(oldBuf);

    std::string actual = capture.str();
    while (!actual.empty() && (actual.back() == '\n' || actual.back() == ' '))
        actual.pop_back();

    std::cout << "\n";
    std::cout << "  Input:    " << description << "\n";
    std::cout << "  Expected: " << expected << "\n";

    if (!ok) {
        std::cout << "  Actual:   [ERROR] " << error << "\n";
        std::cout << "  Status:   FAIL\n";
        stats.failed++;
    } else {
        std::cout << "  Actual:   " << actual << "\n";
        if (actual == expected) {
            std::cout << "  Status:   OK\n";
            stats.passed++;
        } else {
            std::cout << "  Status:   MISMATCH\n";
            stats.failed++;
        }
    }
}

int main(int argc, char *argv[])
{
    mlab::Engine engine;

    size_t totalAllocated = 0;
    size_t peakAllocated = 0;
    size_t allocationCount = 0;

    engine.setAllocator({[&](size_t n) -> void * {
                             totalAllocated += n;
                             allocationCount++;
                             if (totalAllocated > peakAllocated)
                                 peakAllocated = totalAllocated;
                             return ::operator new(n);
                         },
                         [&](void *p, size_t n) {
                             totalAllocated -= n;
                             ::operator delete(p);
                         }});

    mlab::StdLibrary::install(engine);

    TestStats stats;
    auto t0 = std::chrono::steady_clock::now();

    std::cout << "MLab Interpreter -- Feature Showcase\n";
    std::cout << "====================================\n";

    // 1
    printSection("Arithmetic");

    runTest(engine, "disp(2 + 3 * 4 - 1)", "13", stats);
    runTest(engine, "disp((2 + 3) * (4 - 1))", "15", stats);
    runTest(engine, "disp(2 ^ 3 ^ 2)", "512", stats);
    runTest(engine, "disp(mod(17, 5))", "2", stats);
    runTest(engine, "disp(floor(3.7))", "3", stats);
    runTest(engine, "disp(ceil(-2.3))", "-2", stats);

    // 2
    printSection("Matrices");

    runSetup(engine, "A = [1 2 3; 4 5 6; 7 8 9];", "A = [1 2 3; 4 5 6; 7 8 9]", stats);
    runTest(engine, "disp(A(2, 3))", "6", stats);

    runSetup(engine, "B = [9 8 7; 6 5 4; 3 2 1];", "B = [9 8 7; 6 5 4; 3 2 1]", stats);

    runTestBlock(engine, "disp(A + B)", "A + B", "[10 10 10; 10 10 10; 10 10 10]", stats);

    runTestBlock(engine, "disp(A .* B)", "A .* B", "[9 16 21; 24 25 24; 21 16 9]", stats);

    // 3
    printSection("Special Matrices");

    runTest(engine, "disp(eye(3))", "[1 0 0; 0 1 0; 0 0 1]", stats);

    runTest(engine, "disp(zeros(2))", "[0 0; 0 0]", stats);

    runTest(engine, "disp(ones(1, 4))", "[1 1 1 1]", stats);

    runTest(engine, "disp(linspace(0, 1, 5))", "[0 0.25 0.5 0.75 1]", stats);

    // 4
    printSection("Control Flow");

    runTestBlock(engine,
                 R"(
        total = 0;
        for i = 1:100
            total = total + i;
        end
        disp(total)
    )",
                 "sum(1:100)",
                 "5050",
                 stats);

    runTestBlock(engine,
                 R"(
        n = 1;
        while n < 1000
            n = n * 2;
        end
        disp(n)
    )",
                 "first power of 2 >= 1000",
                 "1024",
                 stats);

    runTestBlock(engine,
                 R"(
        val = 42;
        if val > 100
            disp('big')
        elseif val > 10
            disp('medium')
        else
            disp('small')
        end
    )",
                 "if/elseif/else with val=42",
                 "medium",
                 stats);

    // 5
    printSection("Functions & Recursion");

    runSetup(engine,
             R"(
        function y = factorial(n)
            if n <= 1
                y = 1;
            else
                y = n * factorial(n - 1);
            end
        end
    )",
             "define factorial(n)",
             stats);

    runTest(engine, "disp(factorial(10))", "3628800", stats);
    runTest(engine, "disp(factorial(0))", "1", stats);

    runSetup(engine,
             R"(
        function r = fib(n)
            if n <= 1
                r = n;
            else
                r = fib(n-1) + fib(n-2);
            end
        end
    )",
             "define fib(n)",
             stats);

    runTestBlock(engine,
                 R"(
        result = [];
        for k = 0:9
            result = [result, fib(k)];
        end
        disp(result)
    )",
                 "fib(0)..fib(9)",
                 "[0 1 1 2 3 5 8 13 21 34]",
                 stats);

    // 6
    printSection("Multiple Return Values");

    runSetup(engine,
             R"(
        function [mn, mx] = minmax(v)
            mn = v(1);
            mx = v(1);
            for i = 2:length(v)
                if v(i) < mn
                    mn = v(i);
                end
                if v(i) > mx
                    mx = v(i);
                end
            end
        end
    )",
             "define [mn, mx] = minmax(v)",
             stats);

    runTestBlock(engine,
                 R"(
        [lo, hi] = minmax([5 3 9 1 7]);
        disp(lo)
    )",
                 "min of [5 3 9 1 7]",
                 "1",
                 stats);

    runTestBlock(engine,
                 R"(
        [lo, hi] = minmax([5 3 9 1 7]);
        disp(hi)
    )",
                 "max of [5 3 9 1 7]",
                 "9",
                 stats);

    // 7
    printSection("Structures");

    runSetup(engine,
             R"(
        person.name  = 'Alice';
        person.age   = 30;
        person.score = [95 87 92];
    )",
             "create person struct",
             stats);

    runTest(engine, "disp(person.name)", "Alice", stats);
    runTest(engine, "disp(person.age)", "30", stats);

    runSetup(engine,
             R"(
        point.x = 3;
        point.y = 4;
        point.dist = sqrt(point.x^2 + point.y^2);
    )",
             "create point struct with computed dist",
             stats);

    runTest(engine, "disp(point.dist)", "5", stats);

    // 8
    printSection("Nested Structures");

    runSetup(engine,
             R"(
        car.make = 'Toyota';
        car.year = 2024;
        car.engine.horsepower = 203;
        car.engine.type = 'hybrid';
        car.engine.cylinders = 4;
    )",
             "create nested car struct",
             stats);

    runTest(engine, "disp(car.make)", "Toyota", stats);
    runTest(engine, "disp(car.engine.horsepower)", "203", stats);
    runTest(engine, "disp(car.engine.type)", "hybrid", stats);

    // 9
    printSection("Complex Numbers");

    runSetup(engine, "z1 = 3 + 4i; z2 = 1 - 2i;", "z1 = 3+4i, z2 = 1-2i", stats);

    runTest(engine, "disp(abs(z1))", "5", stats);
    runTest(engine, "disp(real(z1))", "3", stats);
    runTest(engine, "disp(imag(z1))", "4", stats);

    // 10
    printSection("Strings");

    runTest(engine, "disp(upper('hello'))", "HELLO", stats);
    runTest(engine, "disp(lower('MATLAB'))", "matlab", stats);
    runTest(engine, "disp(length('OpenAI'))", "6", stats);
    runTest(engine, "disp(strcmp('test', 'test'))", "1", stats);
    runTest(engine, "disp(strcmp('abc', 'xyz'))", "0", stats);

    // 11
    printSection("Math Functions");

    runTest(engine, "disp(sqrt(144))", "12", stats);
    runTest(engine, "disp(abs(-7))", "7", stats);
    runTest(engine, "disp(log(exp(5)))", "5", stats);

    // 12
    printSection("Algorithm: Bubble Sort");

    runSetup(engine,
             R"(
        function result = bubbleSort(arr)
            n = length(arr);
            swaps = 0;
            for i = 1:n-1
                for j = 1:n-i
                    if arr(j) > arr(j+1)
                        temp = arr(j);
                        arr(j) = arr(j+1);
                        arr(j+1) = temp;
                        swaps = swaps + 1;
                    end
                end
            end
            result.sorted = arr;
            result.swaps  = swaps;
        end
    )",
             "define bubbleSort(arr) -> struct",
             stats);

    runSetup(engine,
             "info = bubbleSort([64 34 25 12 22 11 90]);",
             "sort [64 34 25 12 22 11 90]",
             stats);

    runTest(engine, "disp(info.sorted)", "[11 12 22 25 34 64 90]", stats);

    // 13
    printSection("Algorithm: Sieve of Eratosthenes");

    runSetup(engine,
             R"(
        function primes = sieve(limit)
            is_prime = ones(1, limit);
            is_prime(1) = 0;
            for i = 2:floor(sqrt(limit))
                if is_prime(i)
                    for j = i*i:i:limit
                        is_prime(j) = 0;
                    end
                end
            end
            count = 0;
            for i = 1:limit
                if is_prime(i)
                    count = count + 1;
                end
            end
            primes = zeros(1, count);
            idx = 1;
            for i = 1:limit
                if is_prime(i)
                    primes(idx) = i;
                    idx = idx + 1;
                end
            end
        end
    )",
             "define sieve(limit)",
             stats);

    runTest(engine, "disp(sieve(30))", "[2 3 5 7 11 13 17 19 23 29]", stats);

    runTest(engine, "disp(length(sieve(50)))", "15", stats);

    // 14
    printSection("Vector Operations");

    runSetup(engine, "v = 1:10;", "v = 1:10", stats);
    runTest(engine, "disp(sum(v))", "55", stats);
    runTest(engine, "disp(min(v))", "1", stats);
    runTest(engine, "disp(max(v))", "10", stats);

    // Results
    auto t1 = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "\n====================================\n";
    std::cout << "Results\n";
    std::cout << "====================================\n\n";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Tests:       " << stats.total << "\n";
    std::cout << "  Passed:      " << stats.passed << "\n";
    std::cout << "  Failed:      " << stats.failed << "\n";
    std::cout << "  Time:        " << elapsed << " ms\n";
    std::cout << "  Memory now:  " << totalAllocated << " bytes\n";
    std::cout << "  Memory peak: " << peakAllocated << " bytes\n";
    std::cout << "  Allocations: " << allocationCount << "\n\n";

    if (stats.failed == 0)
        std::cout << "  All tests passed.\n\n";
    else
        std::cout << "  Some tests failed.\n\n";

    // REPL
    if (argc > 1 && std::string(argv[1]) == "--repl") {
        std::cout << "====================================\n";
        std::cout << "Interactive REPL (type 'quit' to exit)\n";
        std::cout << "====================================\n\n";

        std::string line;
        while (true) {
            std::cout << ">> ";
            if (!std::getline(std::cin, line))
                break;
            if (line == "quit" || line == "exit")
                break;
            if (line.empty())
                continue;
            try {
                engine.eval(line);
            } catch (const std::exception &e) {
                std::cerr << "Error: " << e.what() << "\n";
            }
        }
        std::cout << "\nGoodbye!\n";
    }

    return stats.failed > 0 ? 1 : 0;
}
