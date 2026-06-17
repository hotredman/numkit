# numkit benchmarks

Two complementary benchmark worlds.

## 1. C++ micro-benches — the `numkit_bench` executable

[Google Benchmark](https://github.com/google/benchmark)-based, one executable.
Each source registers `BM_*` functions that time C++ kernels (or, in
`interpreter/`, the engine itself) in-process.

### Layout

Sources are grouped to mirror `src/` layering; each directory has a
`CMakeLists.txt` that does `target_sources(numkit_bench PRIVATE ...)`.

| Directory | What it benches |
|---|---|
| `math/` | base **math** layer — elementwise, interp, reductions, setops |
| `lang/` | base **lang** layer — binary ops, matmul, manipulation, sort |
| `interpreter/` | the engine itself — native-C++ vs bytecode-VM vs TreeWalker (see `iir_filter_bench.cpp`) |
| `src/toolboxes/<lib>/benchmarks/` | per-toolbox kernels, **co-located** with the toolbox (signal, stats, image, …) |

(`math/` + `lang/` replaced the old `benchmarks/builtin/` umbrella on
2026-06-17, matching the `builtin`→`math`/`lang`/`runtime` src split.)

### Build & run

```sh
cmake --preset bench          # or any preset with -DNUMKIT_BUILD_BENCHMARKS=ON
cmake --build build/bench --target numkit_bench --config Release

# all benches
build/bench/benchmarks/Release/numkit_bench.exe
# one family
build/bench/benchmarks/Release/numkit_bench.exe --benchmark_filter=BM_Biquad.*
# machine-readable
build/bench/benchmarks/Release/numkit_bench.exe \
    --benchmark_out=results.json --benchmark_out_format=json
```

Presets: `bench` (scalar), `bench-simd` (Highway SIMD), `bench-clang`,
`bench-simd-clang`, `bench-simd-threads`, `bench-wasm` (run via
`node numkit_bench.js`).

### SIMD A/B comparison

`bench_simd.sh` / `bench_simd.bat` build the scalar and SIMD presets, run both,
and `compare_simd.py` diffs the two JSON outputs into a speedup table.

## 2. M-script benches — `benchmarks/m/`

Hand-run `.m` scripts that measure performance from the language side
(interpreter throughput and library functions), not individual C++ kernels:

| Script | Measures |
|---|---|
| `benchmark_interp.m` | raw interpreter speed — loops, calls, indexing, scalar math |
| `benchmark_simd.m` | vectorised library functions (abs/sin/cos/exp/log, `+ - .* ./`) |
| `benchmark_simd_inplace.m` | same kernels writing a pre-allocated buffer via `z(:) = rhs` |
| `benchmark_grow.m` | incremental array-grow patterns |

Run with the smoke runner (each starts with `clear`):

```sh
build/desktop-fast/tests/smoke/Release/numkit_smoke.exe benchmarks/m/benchmark_interp.m
```

The interpreter-overhead theme of `benchmark_interp.m` is being migrated into
permanent, CI-able Google Benchmark form under `interpreter/`.
