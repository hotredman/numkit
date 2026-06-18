// benchmarks/interpreter/iir_filter_bench.cpp
//
// Interpreter-overhead benchmark: a 2nd-order IIR filter (biquad, Direct
// Form I) run over a long sample. The recursion makes y[n] depend on y[n-1]
// and y[n-2] -- a true cross-iteration dependency that CANNOT be vectorised
// away -- so it isolates the raw cost of a sequential scalar loop in each
// execution path:
//
//   BM_Biquad_NativeCpp          - hand-written C++ loop (the speed-of-light)
//   BM_Biquad_NumkitFilter       - numkit filter() builtin (C++ kernel, via eval)
//   BM_Biquad_NumkitLoop_VM      - the same loop in M-code on the bytecode VM
//   BM_Biquad_NumkitLoop_TreeWalker - ... on the TreeWalker
//
// All four produce identical output. ns/sample = 1e9 / items_per_second
// (read the items_per_second column). Snapshot 2026-06-17, bench preset,
// N = 131072, after loop opts #1 (indexed scalar fast-path) + #2 (MULADD
// fusion):  native ~1.6 | filter() ~5.5 | VM-loop ~148 | TreeWalker ~321
// ns/sample (VM-loop was ~185 before #1+#2). Reference: MATLAB R2025b JIT loop
// ~4.0, MATLAB filter() ~7.8 -- numkit's filter() (5.5) beats MATLAB's; the VM
// loop is ~37x its JIT, the residual interpreter-vs-JIT gap that only a loop JIT
// would close. The M-loop body is dispatch-bound (~15 opcodes/sample); the
// empty for-loop is ~2 ns/iteration, so the loop machinery is not the cost.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>

#include <benchmark/benchmark.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr std::size_t kN = 1u << 17;  // 131072 samples (keep in sync with kSetup)
// Stable 2nd-order Butterworth low-pass (textbook), a(1) normalised to 1.
constexpr double kB0 = 0.0675, kB1 = 0.1349, kB2 = 0.0675;
constexpr double kA1 = -1.1430, kA2 = 0.4128;

void BM_Biquad_NativeCpp(benchmark::State &state)
{
    std::vector<double> x(kN), y(kN);
    for (std::size_t n = 0; n < kN; ++n) x[n] = std::sin(0.01 * double(n + 1));
    for (auto _ : state) {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (std::size_t n = 0; n < kN; ++n) {
            double xn = x[n];
            double yn = kB0 * xn + kB1 * x1 + kB2 * x2 - kA1 * y1 - kA2 * y2;
            y[n] = yn;
            x2 = x1; x1 = xn;
            y2 = y1; y1 = yn;
        }
        benchmark::DoNotOptimize(y.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(std::int64_t(state.iterations()) * kN);
}
BENCHMARK(BM_Biquad_NativeCpp);

// Workspace + coefficients set up once; the timed string is just the loop
// (state reset + recursion over the pre-allocated y). Per-iteration parse is
// negligible against N = 131072 samples of work.
const char *kSetup =
    "N = 131072;"
    "x = sin(0.01*(1:N));"
    "b0=0.0675; b1=0.1349; b2=0.0675; a1=-1.1430; a2=0.4128;"
    "y = zeros(1,N);";

const char *kLoop =
    "x1=0; x2=0; y1=0; y2=0;"
    "for n=1:N;"
    "  xn = x(n);"
    "  yn = b0*xn + b1*x1 + b2*x2 - a1*y1 - a2*y2;"
    "  y(n) = yn;"
    "  x2=x1; x1=xn; y2=y1; y1=yn;"
    "end";

void runLoop(benchmark::State &state, numkit::Engine::Backend backend)
{
    numkit::StandardEngine e;
    e.setBackend(backend);
    e.eval(kSetup);
    for (auto _ : state)
        e.eval(kLoop);
    state.SetItemsProcessed(std::int64_t(state.iterations()) * kN);
}

void BM_Biquad_NumkitLoop_VM(benchmark::State &state)
{
    runLoop(state, numkit::Engine::Backend::VM);
}
BENCHMARK(BM_Biquad_NumkitLoop_VM);

void BM_Biquad_NumkitLoop_TreeWalker(benchmark::State &state)
{
    runLoop(state, numkit::Engine::Backend::TreeWalker);
}
BENCHMARK(BM_Biquad_NumkitLoop_TreeWalker);

void BM_Biquad_NumkitFilter(benchmark::State &state)
{
    numkit::StandardEngine e;
    e.eval("import compat.*;");  // filter() lives in the signal namespace
    e.eval(kSetup);
    e.eval("b=[0.0675 0.1349 0.0675]; a=[1 -1.1430 0.4128];");
    for (auto _ : state)
        e.eval("yf = filter(b,a,x);");
    state.SetItemsProcessed(std::int64_t(state.iterations()) * kN);
}
BENCHMARK(BM_Biquad_NumkitFilter);

} // namespace
