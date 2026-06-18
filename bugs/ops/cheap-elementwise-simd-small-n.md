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

## Fused affine/abs/sq fix + root cause (2026-06-18)

Gating the same way at first did NOT help (gated zone stuck ~0.5–0.8×). Root-caused
it with `cl /Qvec-report:2` on a structural repro: **the difference was the
`parallel_for` lambda capture.** The binary wrappers capture `[=]` (by value); the
fused wrappers captured `[&]` (by reference). A `[&]` capture of the function's
`out`/`x` pointers makes MSVC treat them as address-escaped → conservative alias
analysis → the *earlier* gate loop `out[i] = scale*x[i]+offset` fails to vectorize
(vec-report **reason 1104**), running scalar at ~half speed. With `[=]` the same
gate loop vectorizes (vec-report C5001). The loop body itself was never the
problem — it vectorizes fine standalone under `/fp:precise`.

Fix: gate `fused{Affine,AbsAffine,AbsShiftDiv,AbsDiff,SqAffine,SqShiftDiv,SqDiff}`
(`fused_{affine,abs,sq}_highway.cpp`) **and** switch their `parallel_for` lambdas
`[&]` → `[=]`. A/B at N=1024 / 16384 / 65536, before → after:

```
FusedAffine     0.78x → 0.99x / 1.11x / 0.95x
FusedAbsAffine  0.68x → 0.98x / 1.02x / 1.00x
FusedSqAffine   0.69x → 0.98x / 1.03x / 1.01x
```

Bit-identical (suite 12178/12177; the inline body == the Highway scalar tail).
The branchy/transcendental fused (clamp, soft-threshold, sqrt-sum-sq, trans) win
3–27× and were left ungated. **Lesson:** an HWY dispatcher wrapping its body in a
`parallel_for` must capture `[=]`, or any inline fast-path loop sharing its
pointers won't vectorize on MSVC.

## Structural hardening — gate delegates to a shared scalar kernel (2026-06-18)

The `[=]`-vs-`[&]` capture rule above is a *discipline*: a future inline gate
that reverts to `[&]` would silently re-crater. Made it structural instead — the
small-N gate no longer inlines a loop; it **calls a shared scalar kernel** that
lives in its own always-compiled, lambda-free TU:

- `src/ops/src/fused/fused_scalar.cpp` → `numkit::ops::fused*Scalar` (7 kernels:
  Affine, AbsAffine, AbsShiftDiv, AbsDiff, SqAffine, SqShiftDiv, SqDiff)
- `src/ops/src/binary_scalar.cpp` → `numkit::ops::detail::{plus,minus,times,rdivide}Scalar`

Both the portable fallback (`fused_kernels_portable.cpp` / `binary_ops_portable.cpp`)
and the Highway small-N gate now forward to these. Two wins at once:

1. **Footgun #1 gone structurally** — the auto-vectorizable loop lives in a TU
   with no `parallel_for` lambda, so it vectorizes regardless of how *any*
   dispatcher captures its worker. The `[=]` capture is no longer load-bearing
   (kept for consistency). A future gate that delegates can't re-crater.
2. **Footgun #2 gone** — the gate body and the portable fallback are one
   definition, so they can't drift (previously 3 hand-copies that had to stay
   bit-identical: gate, Highway tail, portable).

`NUMKIT_NOINLINE` (`numkit/ops/compiler.hpp`) keeps the body from being merged
back into an escaping-lambda context under IPO/LTCG (off today → the separate TU
already suffices). A/B re-verified after the move — all gated families ≥0.96× at
every N (no crater), big-N SIMD wins intact (Plus/16384 2.26×, Times/16384
2.19×); suite 12178/12177 bit-identical. **The durable invariant: a small-N gate
delegates to the shared scalar kernel — it never inlines its own loop.**

## References

- Shared scalar kernels: `src/ops/src/fused/fused_scalar.cpp`,
  `src/ops/src/binary_scalar.cpp`; `NUMKIT_NOINLINE` in
  `src/ops/include/numkit/ops/compiler.hpp`.
- Gate + dispatch: `src/ops/src/fused/fused_{affine,abs,sq}_highway.cpp`,
  `src/ops/src/binary_ops_highway.cpp`;
  `src/ops/include/numkit/ops/parallel_for.hpp` (`kSimdInlineThreshold`,
  `kCheapElementwiseThreshold`).
- Bench: `src/lang/benchmarks/binaryops_bench.cpp` + `src/ops/benchmarks/fused_bench.cpp`,
  runner `benchmarks/simd/`.
- Memory: `feedback_simd_dynamic_dispatch_small_n`, `feedback_threading_dispatch_too_costly`.

- `src/ops/src/binary_ops_highway.cpp` (the `plus/minus/times/rdivide` Loop wrappers),
  `src/ops/include/numkit/ops/parallel_for.hpp` (`kCheapElementwiseThreshold`).
- Bench: `src/lang/benchmarks/binaryops_bench.cpp`, runner `benchmarks/simd/`.
- Memory: `feedback_simd_dynamic_dispatch_small_n`, `feedback_threading_dispatch_too_costly`.
