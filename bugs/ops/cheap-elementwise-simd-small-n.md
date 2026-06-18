# ops.{plus,minus,times,rdivide} — Highway SIMD slower than scalar at small N (MSVC)

- **Status:** ✅ FIXED (2026-06-18)
- **Kind:** perf
- **Severity:** P3 (internal SIMD-vs-scalar regression; cache-resident sizes only — large/DRAM arrays unaffected)
- **Slowdown:** up to ~8× — the Highway SIMD build runs the cheap element-wise
  ops at **0.10–0.30× of the scalar build** for cache-resident N on native MSVC.
- **Found:** 2026-06-18 via the `benchmarks/simd/` A/B sweep.

## Symptom

On the native MSVC build, the Highway-SIMD versions of the cheapest element-wise
binary ops are **slower than the scalar build** for small/cache-resident arrays,
recovering only at DRAM-size N. The fused affine kernels show a milder version
of the same.

## Repro

```
benchmarks/simd/bench_simd.sh   # bench (scalar) vs bench-simd (Highway), MSVC

family          N=1024 (L1)   N=4194304 (DRAM)
Plus              0.10x            1.18x
Minus             0.11x            1.08x
Times             0.12x            1.41x
Rdivide           0.30x            1.17x
PlusKernel        0.25x            1.08x   # pure kernel, no alloc → real overhead
FusedAffine       0.78x            0.79x
```

Speedup = scalar_time / simd_time; <1 means the SIMD build is slower. The crater
is worst at L1/L2 (≤ 65536 elems) and breaks even around N≈262144.

**Cross-platform control (WASM, clang, wasm128):** the crater does NOT reproduce —
the same ops are ~1.0–1.2× on WASM:

```
                N=1024     N=4194304
Plus            1.09x        1.04x
Times           1.07x        1.05x
Rdivide         1.19x        1.00x
```

→ the regression is **native/MSVC-specific**, not a kernel-body defect.

## Root cause

With `NUMKIT_WITH_THREADS` off (both bench presets), `parallel_for` is a direct
call, so the only delta from the scalar build is that the Highway path goes
through `HWY_DYNAMIC_DISPATCH` — a runtime-ISA **indirect call into a
separately-compiled, non-inlinable** SIMD function — whereas the portable build
lets MSVC **inline + auto-vectorize** the scalar loop in place. At cache-resident
N the indirect-call / no-inline overhead dwarfs the tiny per-call work. WASM has
a single target (wasm128) → Highway dispatches **statically** (inlinable) → no
overhead → no crater. Transcendentals/reductions are immune (the per-call
overhead is negligible against their heavy bodies, and MSVC can't auto-vectorize
a libm call) — they win 3–20× on both platforms.

## Suggested fix

Size-gate the cheap element-wise ops *before* `HWY_DYNAMIC_DISPATCH`: below a
threshold (~256K elems) run an inline scalar loop (MSVC auto-vectorizes it,
bit-identical for `+ - .* ./` — one IEEE rounding each), use Highway only for
large arrays where its body wins on memory traffic. Disable the gate on WASM
(`__EMSCRIPTEN__`, threshold 0) since static dispatch there is free and Highway
already wins small. Scope: `src/ops/src/binary_ops_highway.cpp` (4 wrappers) +
one constant. The simple `fused*Affine` shapes show a milder version → follow-up.

## Fix landed (2026-06-18)

Added `kSimdInlineThreshold` (`parallel_for.hpp`; native 256K, WASM 0) and gated
`plus/minus/times/rdivide` (`binary_ops_highway.cpp`): below the threshold run an
inline scalar loop, above use Highway. Native A/B at N=1024 (scalar→SIMD speedup),
before → after:

```
Plus     0.10x → 0.97x    Minus   0.11x → 1.42x
Times    0.12x → 1.54x    Rdivide 0.30x → 2.26x    PlusKernel 0.25x → 1.29x
```

Crater gone (the inline loop in the AVX2-compiled SIMD build even out-widths the
SSE2 portable baseline). Big-N path unchanged (still Highway). Full suite
12178/12177 bit-identical (the inline `+ - .* ./` is one IEEE op, == the Highway
lane op). WASM untouched (gate compile-disabled).

**Fused affine/abs/sq follow-up — attempted, did NOT work, reverted.** Gating the
simpler `fused{Affine,AbsAffine,AbsDiff,AbsShiftDiv,SqAffine,SqDiff,SqShiftDiv}`
the same way (inline scalar below the threshold) did not help: in the gated zone
the SIMD build stayed ~0.5–0.8× of the scalar build (consistent across N=1k–64k,
not noise). The inline scalar loop apparently does NOT auto-vectorize inside the
fused Highway TUs on MSVC — unlike the binary `*_highway.cpp` TUs where the same
pattern recovered to ~1×. Reverted (no point shipping a no-op branch). A real fix
would need a different vehicle (a dedicated auto-vectorized scalar TU, or a
`#pragma`/attribute forcing vectorization of the gate loop) — deferred: the fused
crater is milder and only on the VM-expression-fusion path. Why MSVC vectorizes
the gate in `binary_ops_highway.cpp` but not the `fused_*_highway.cpp` TUs is the
open question for that follow-up.

## References

- `src/ops/src/binary_ops_highway.cpp` (the `plus/minus/times/rdivide` Loop wrappers),
  `src/ops/include/numkit/ops/parallel_for.hpp` (`kCheapElementwiseThreshold`).
- Bench: `src/lang/benchmarks/binaryops_bench.cpp`, runner `benchmarks/simd/`.
- Memory: `feedback_simd_dynamic_dispatch_small_n`, `feedback_threading_dispatch_too_costly`.
