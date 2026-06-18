// src/codegen/benchmarks/biquad_codegen_bench.cpp
//
// M0 — measure the codegen "prize" before building the inference engine.
//
// The biquad scalar recurrence (y[n] depends on y[n-1], y[n-2] — a true
// cross-iteration dependency that CANNOT be vectorised) is the canonical
// interpreter-overhead kernel. The core bench (iir_filter_bench.cpp)
// already pins raw-array native C++ vs the VM vs MATLAB. This adds the
// ONE point specific to our codegen design:
//
//   the transpiler does NOT emit a raw-array loop. For `y = <biquad>(x)`
//   with x, y inferred as double arrays, it emits an unboxed `double`
//   recurrence whose input/output ARRAYS are numkit Value containers
//   accessed through doubleData()/doubleDataMut() (the array
//   representation tier — see src/codegen/DESIGN.md §5).
//
// This benchmark confirms the Value-container array tier keeps native
// speed: "array stays a Value, elements/scalars are unboxed" should lose
// nothing versus a raw double[] loop. The Value->raw-pointer hoist
// happens once per call (as the emitter would emit it), not per sample.
//
// ns/sample = 1e9 / items_per_second. Compare against the core bench's
// BM_Biquad_NumkitLoop_VM (the boxed interpreter we are escaping) and
// BM_Biquad_NativeCpp (raw-array speed of light).
//
// Snapshot 2026-06-18, Arrow Lake, desktop-fast Release, N=131072
// (ns/sample): ValueIO (this) 1.56 | NativeCpp 1.59 | MATLAB JIT loop
// 2.70 | numkit filter() 5.85 | MATLAB filter() 7.22 | VM loop 151.4 |
// TreeWalker 336.6. The transpiler-faithful output is ~97x the VM and
// ~1.7x faster than MATLAB's JIT loop; the Value-container array tier
// costs nothing vs raw double[].

#include <numkit/value/value.hpp>
#include <numkit/value/value_type.hpp>

#include <benchmark/benchmark.h>

#include <cmath>
#include <cstdint>

namespace {

constexpr std::size_t kN = 1u << 17;  // 131072 — same as iir_filter_bench
constexpr double kB0 = 0.0675, kB1 = 0.1349, kB2 = 0.0675;
constexpr double kA1 = -1.1430, kA2 = 0.4128;

// Transpiler-faithful: x and y are Value arrays (the array tier); the
// inner recurrence is pure unboxed double over their raw buffers.
void BM_Biquad_Codegen_ValueIO(benchmark::State &state)
{
    numkit::Value x = numkit::Value::matrix(1, kN, numkit::ValueType::DOUBLE, nullptr);
    numkit::Value y = numkit::Value::matrix(1, kN, numkit::ValueType::DOUBLE, nullptr);
    {
        double *xd = x.doubleDataMut();
        for (std::size_t n = 0; n < kN; ++n) xd[n] = std::sin(0.01 * double(n + 1));
    }

    for (auto _ : state) {
        const double *xp = x.doubleData();    // Value -> raw ptr, hoisted once
        double *yp = y.doubleDataMut();
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (std::size_t n = 0; n < kN; ++n) {
            const double xn = xp[n];
            const double yn = kB0 * xn + kB1 * x1 + kB2 * x2 - kA1 * y1 - kA2 * y2;
            yp[n] = yn;
            x2 = x1; x1 = xn;
            y2 = y1; y1 = yn;
        }
        benchmark::DoNotOptimize(yp);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(std::int64_t(state.iterations()) * kN);
}
BENCHMARK(BM_Biquad_Codegen_ValueIO);

} // namespace
