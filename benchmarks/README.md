# numkit benchmarks

Two complementary benchmark worlds.

## 1. C++ micro-benches — the `numkit_bench` executable

[Google Benchmark](https://github.com/google/benchmark)-based, one executable.
Each source registers `BM_*` functions that time C++ kernels (or, for the core
engine, the interpreter itself) in-process.

### Layout

Bench sources are **co-located with the module they measure** — each module has
a `benchmarks/` dir with a `CMakeLists.txt` that does
`target_sources(numkit_bench PRIVATE ...)`, the same layout as
`src/<module>/tests/`. This `benchmarks/` directory owns the `numkit_bench`
target and pulls them all in.

| Bench sources | What they bench |
|---|---|
| `src/math/benchmarks/` | base **math** layer — elementwise, interp, reductions, setops |
| `src/lang/benchmarks/` | base **lang** layer — binary ops, matmul, manipulation, sort |
| `src/core/benchmarks/` | the engine itself — native-C++ vs bytecode-VM vs TreeWalker (`iir_filter_bench.cpp`) |
| `src/toolboxes/<lib>/benchmarks/` | per-toolbox kernels (signal, stats, image, …) |

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

### SIMD A/B comparison — `benchmarks/simd/`

`simd/bench_simd.sh` / `simd/bench_simd.bat` build the scalar and SIMD presets,
run both, and `simd/compare_simd.py` diffs the two JSON outputs into a speedup
table.

The `FILTER` in those runners covers every numkit family with a portable-vs-
Highway split: unary math (abs/sin/cos/tan/exp/log/sqrt/round/floor/ceil/fix/
isfinite), binary ops (plus/minus/times/rdivide/mod + the six comparisons),
matmul, fft, the SIMD reductions (any/all/var/std/cumsum) and the ops/fused
one-pass kernels (`BM_Fused*`). Keep it in sync when a new SIMD kernel lands.

## 2. M-script benches — `benchmarks/mscripts/`

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
build/desktop-fast/apps/numkit/Release/numkit.exe benchmarks/mscripts/benchmark_interp.m
```

These use `import compat.*` and so are numkit-only.

### Cross-engine reference — `src/core/benchmarks/iir_filter_ref.m`

Co-located with its C++ companion `iir_filter_bench.cpp`. Deliberately
import-free and `try/catch`-guards `filter()`, so the identical file runs in
MATLAB / Octave / numkit to compare the scalar-loop cost:

```sh
matlab -batch "run('src/core/benchmarks/iir_filter_ref.m')"   # ~3.9 ns/sample (JIT)
octave-cli src/core/benchmarks/iir_filter_ref.m
build/desktop-fast/apps/numkit/Release/numkit.exe src/core/benchmarks/iir_filter_ref.m  # ~150
```

The interpreter-overhead theme of `benchmark_interp.m` is being migrated into
permanent, CI-able Google Benchmark form under `src/core/benchmarks/`.
