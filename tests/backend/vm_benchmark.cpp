// tests/vm_benchmark.cpp
//
// Direct timing comparison: TreeWalker vs Bytecode VM
// Run with: ./m_tests --gtest_filter="VMBenchmark.*"

#include <numkit/m/frontend/MCompiler.hpp>
#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/frontend/MLexer.hpp>
#include <numkit/m/frontend/MParser.hpp>
#include "MStdLibrary.hpp"
#include <numkit/m/backend/MVM.hpp>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>

using namespace numkit::m;

class VMBenchmark : public ::testing::Test
{
public:
    Engine engine;
    Compiler compiler{engine};
    VM vm{engine};

    void SetUp() override
    {
        StdLibrary::install(engine);
        engine.setOutputFunc([](const std::string &) {}); // suppress output
    }

    // Run via TreeWalker (Engine::eval)
    double runTreeWalker(const std::string &code) { return engine.eval(code).toScalar(); }

    // Run via Bytecode VM
    double runVM(const std::string &code)
    {
        Lexer lexer(code);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto ast = parser.parse();
        auto chunk = compiler.compile(ast.get());
        vm.setCompiledFuncs(&compiler.compiledFuncs());
        return vm.execute(chunk).toScalar();
    }

    struct BenchResult
    {
        double twTime;
        double vmTime;
        double speedup;
        double result;
    };

    BenchResult benchmark(const std::string &name, const std::string &code, int runs = 3)
    {
        double twBest = 1e9, vmBest = 1e9;
        double result = 0;

        for (int r = 0; r < runs; ++r) {
            // TreeWalker
            auto t0 = std::chrono::high_resolution_clock::now();
            double twResult = runTreeWalker(code);
            auto t1 = std::chrono::high_resolution_clock::now();
            double twMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
            if (twMs < twBest)
                twBest = twMs;

            // VM
            auto t2 = std::chrono::high_resolution_clock::now();
            double vmResult = runVM(code);
            auto t3 = std::chrono::high_resolution_clock::now();
            double vmMs = std::chrono::duration<double, std::milli>(t3 - t2).count();
            if (vmMs < vmBest)
                vmBest = vmMs;

            result = vmResult;

            // Verify same result
            EXPECT_NEAR(twResult, vmResult, 1e-6) << name << ": TreeWalker and VM results differ";
        }

        double speedup = twBest / vmBest;

        std::cerr << "  " << name << ": TW=" << twBest << "ms"
                  << "  VM=" << vmBest << "ms"
                  << "  speedup=" << speedup << "x"
                  << "  result=" << result << "\n";

        return {twBest, vmBest, speedup, result};
    }
};

TEST_F(VMBenchmark, NestedLoop)
{
    auto r = benchmark("Nested loop 200x200", R"(
        s = 0;
        for i = 1:200
            for j = 1:200
                s = s + 1;
            end
        end
        s;
    )");
    EXPECT_DOUBLE_EQ(r.result, 40000.0);
}

TEST_F(VMBenchmark, ScalarMath)
{
    auto r = benchmark("Scalar math 20000", R"(
        x = 0;
        for i = 1:20000
            x = x + i * 0.5 - i / 3.0 + i * i * 0.001;
        end
        x;
    )");
    EXPECT_TRUE(r.result != 0);
}

TEST_F(VMBenchmark, FunctionCalls)
{
    auto r = benchmark("Function calls 5000", R"(
        function r = increment(x)
            r = x + 1;
        end
        v = 0;
        for i = 1:5000
            v = increment(v);
        end
        v;
    )");
    EXPECT_DOUBLE_EQ(r.result, 5000.0);
}

TEST_F(VMBenchmark, RecursiveFib)
{
    auto r = benchmark("Recursive fib(20)", R"(
        function r = fib(n)
            if n <= 1
                r = n;
            else
                r = fib(n-1) + fib(n-2);
            end
        end
        fib(20);
    )");
    EXPECT_DOUBLE_EQ(r.result, 6765.0);
}

TEST_F(VMBenchmark, ArrayIndexRW)
{
    auto r = benchmark("Array index r/w 1000", R"(
        x = zeros(1, 1000);
        for i = 1:1000
            x(i) = i;
        end
        s = 0;
        for i = 1:1000
            s = s + x(i);
        end
        s;
    )");
    EXPECT_DOUBLE_EQ(r.result, 500500.0);
}

TEST_F(VMBenchmark, Branching)
{
    auto r = benchmark("Branching 20000", R"(
        c = 0;
        for i = 1:20000
            if mod(i, 3) == 0
                c = c + 1;
            elseif mod(i, 5) == 0
                c = c + 2;
            else
                c = c + 3;
            end
        end
        c;
    )");
    EXPECT_DOUBLE_EQ(r.result, 44001.0);
}

TEST_F(VMBenchmark, Peaks)
{
    auto r = benchmark("Peaks 200x200",
                       R"(
        n = 200;
        Z = zeros(n, n);
        for i = 1:n
            for j = 1:n
                x = -3 + 6 * (j-1) / (n-1);
                y = -3 + 6 * (i-1) / (n-1);
                Z(i,j) = 3*(1-x)^2 * exp(-x^2 - (y+1)^2) - 10*(x/5 - x^3 - y^5) * exp(-x^2 - y^2) - 1/3 * exp(-(x+1)^2 - y^2);
            end
        end
        Z(1,1);
    )",
                       1);
    EXPECT_TRUE(r.result != 0);
}

TEST_F(VMBenchmark, Summary)
{
    std::cerr << "\n=== VM vs TreeWalker Benchmark ===\n";

    auto r1 = benchmark("1. Nested loop 200x200",
                        "s = 0; for i = 1:200; for j = 1:200; s = s + 1; end; end; s;");
    auto r2
        = benchmark("2. Scalar math 20000",
                    "x = 0; for i = 1:20000; x = x + i * 0.5 - i / 3.0 + i * i * 0.001; end; x;");
    auto r3 = benchmark(
        "3. Function calls 5000",
        "function r = inc(x); r = x + 1; end; v = 0; for i = 1:5000; v = inc(v); end; v;");
    auto r4 = benchmark(
        "4. Recursive fib(20)",
        "function r = fib(n); if n <= 1; r = n; else; r = fib(n-1) + fib(n-2); end; end; fib(20);");
    auto r5 = benchmark("5. Array index r/w 1000",
                        "x = zeros(1, 1000); for i = 1:1000; x(i) = i; end; s = 0; for i = 1:1000; "
                        "s = s + x(i); end; s;");
    auto r6 = benchmark("6. Branching 20000",
                        "c = 0; for i = 1:20000; if mod(i, 3) == 0; c = c + 1; elseif mod(i, 5) == "
                        "0; c = c + 2; else; c = c + 3; end; end; c;");
    auto r7
        = benchmark("7. Peaks 200x200",
                    "n = 200; Z = zeros(n, n); for i = 1:n; for j = 1:n; x = -3 + 6*(j-1)/(n-1); y "
                    "= -3 + 6*(i-1)/(n-1); Z(i,j) = 3*(1-x)^2 * exp(-x^2 - (y+1)^2) - 10*(x/5 - "
                    "x^3 - y^5) * exp(-x^2 - y^2) - 1/3 * exp(-(x+1)^2 - y^2); end; end; Z(1,1);",
                    1);

    double twTotal = r1.twTime + r2.twTime + r3.twTime + r4.twTime + r5.twTime + r6.twTime
                     + r7.twTime;
    double vmTotal = r1.vmTime + r2.vmTime + r3.vmTime + r4.vmTime + r5.vmTime + r6.vmTime
                     + r7.vmTime;

    std::cerr << "\n  TOTAL: TW=" << twTotal << "ms  VM=" << vmTotal << "ms"
              << "  speedup=" << (twTotal / vmTotal) << "x\n";
}