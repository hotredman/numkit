# Linear Algebra Performance Plan — Competing with MATLAB

**Goal:** bring `numkit` linear algebra performance to a competitive level
with MATLAB on the same machine. MATLAB is backed by Intel MKL: packed,
register-blocked, cache-blocked, multithreaded kernels running near
hardware peak. A vectorized textbook kernel cannot compete; this plan
builds the missing layers in order of impact.

**Scope:** real+complex double GEMM/GEMV/GER/TRSM/SYRK microkernel stack
in `src/ops/src/blas/`, factorizations routed through it
(`src/toolboxes/linalg/src/`), threading via the existing `parallel_for`
infrastructure (`src/ops`), and measured perf gates against MATLAB.

**Policy:** per `bugs/README.md`, ratio ≥ 3× vs MATLAB on any probed size
is an S-scale perf bug. This plan's targets are stricter (see P6), because
the module's stated goal is competing, not merely avoiding bug reports.

---

## Current state (as of `851f2cd5`, 2026-08-06)

What exists and is honest:

- `src/ops/src/blas/gemm_highway.cpp`: real-double GEMM/GEMV/GER with
  proper Highway infrastructure (`foreach_target`, `HWY_EXPORT`,
  `HWY_DYNAMIC_DISPATCH`, `ScalableTag`, FMA `MulAdd`, 4-vector unroll,
  scalar tails). Axpy-style j–l–i loop nest, **no register blocking, no
  packing, no cache blocking, no threading** — memory-bound on large
  sizes; expect a multiple slower than MKL at n ≥ 512.
- Complex GEMM: **scalar**, lives outside `HWY_NAMESPACE` (autovectorizer
  hope only), and re-splits column `l` of A into `Ar/Ai` inside the `j`
  loop — redundant copy work of order m·k·n, comparable to the FLOP count.
- `trsm`: scalar; only 3 of 16 side/uplo/trans/diag combinations
  implemented; **unsupported combos silently return wrong results** (only
  alpha scaling is applied). No syrk/herk at all.
- Factorizations: blocked LU routes trailing updates through `ops::gemm`
  (real + complex). QR is unblocked scalar Householder; Cholesky is
  blocked but fully scalar (`cholUpperFactorBlocked`). Bench harness
  (Google Benchmark, `numkit_bench.exe`) exists with committed raw output
  in `src/toolboxes/linalg/benchmarks/results/`.

---

## P1 — Register-blocked, packed, cache-blocked GEMM (real double)  [XL, CRITICAL]

Replace the axpy-style kernel with a BLIS-structure GEMM. This is the
foundation everything else reuses; do it first and do it properly.

**Microkernel.** An `mr × nr` tile of C held entirely in vector
registers; A and B streamed from packed buffers with FMA. With Highway
`ScalableTag<double>` (`N = Lanes(d)`), use `mr = 2·N` (two accumulator
rows of vectors) and `nr = 6` — i.e. 12 accumulator vectors, fitting
both AVX2 (N=4 → 8×6 doubles) and AVX-512 (N=8 → 16×6) without spills.
Broadcast B elements (`hn::Set`), FMA into accumulators; loop over kc.
Edge tiles (m % mr, n % nr) via masked loads/stores (`hn::MaskedLoad`)
or a scalar edge kernel — must be correct for every size, test n=513.

**Packing.** Pack A into column-panels of height mr (contiguous
mr×kc blocks) and B into row-panels of width nr (kc×nr). Packing buffers
allocated once per call (or thread-local), aligned to `HWY_ALIGNMENT`.

**Cache blocking.** BLIS loop order: jc over n (block nc, sized for L3),
pc over k (block kc ≈ 256, A-panel resident in L2), ic over m (block
mc ≈ 128–256), then jr/ir microkernel loops. Tune kc/mc/nc once on the
target machine (24-core x86_64, L2 3 MB/core, L3 36.8 MB — see
`benchmarks/results/2026-08-06_x86_64_msvc.txt`).

**Semantics.** Keep BLAS semantics: `beta == 0` ⇒ C not read. Do NOT
skip work when a B element equals 0.0 inside the microkernel path
(preserves NaN/Inf propagation; the current axpy kernel's
`if (blj == 0.0) continue` deviates — remove it in the rewrite).

**Acceptance:**
- Bit-level correctness vs the existing scalar reference on random sizes
  including odd/tail cases (63, 64, 65, 127, 129, 255, 257, 513, 1000)
  with residual < k·eps·‖A‖‖B‖; NaN/Inf propagation test.
- Single-thread `gemm` n=1024: ≥ 4× faster than the current axpy kernel
  (measured, committed raw output). Record achieved GFLOP/s and % of
  single-core FMA peak in the README (target ≥ 60%).

## P2 — Threading via parallel_for  [L, CRITICAL]

MATLAB uses all cores; single-threaded numkit cannot compete on n ≥ 512.

- Reuse the existing `parallel_for` infrastructure from `src/ops` (already
  used by the fused kernels and SoA-SIMD FFT); do not invent a new pool.
- Parallelize the jc loop (independent C column-blocks); each thread gets
  its own B-packing buffer; A-panel packing shared per pc iteration
  (pack once, barrier) or per-thread — measure both, keep the winner.
- Threshold: single-threaded below ~64k FLOP-equivalent (e.g. n < 128 for
  square gemm) to avoid pool overhead on small sizes.
- Determinism note in code comments: FP summation order within a C tile
  is fixed by the microkernel; parallelism across tiles does not change
  per-element results.

**Acceptance:** gemm n=2048 scaling ≥ 12× over the P1 single-thread
number on the 24-core machine (measured, committed); no result change vs
single-thread run (bitwise identical per element).

## P3 — Complex GEMM done properly  [L, HIGH]

Replace the scalar complex path with the 4M split-complex method reusing
the P1 real microkernel stack:

- During packing, split interleaved `std::complex<double>` panels into
  separate real/imag packed buffers (pack ONCE — this removes the current
  per-j re-split, which alone costs ~m·k·n copies).
- Compute `Cr += Ar·Br − Ai·Bi`, `Ci += Ar·Bi + Ai·Br` as 4 packed real
  microkernel passes over the same packed panels; interleave back on the
  C write path (or keep C split per jc block and merge once).
- Lives inside `HWY_NAMESPACE` with dynamic dispatch like the real path.
- Optional later: 3M (Karatsuba) variant — only after 4M lands and gates
  pass; note the reduced numerical robustness of 3M in comments.

**Acceptance:** complex gemm n=512 within 2.5× of the real-double P1+P2
time at the same size (theoretical factor is 4 FLOPs but same memory
traffic after packing; 2.5× is the honest target with 4M); correctness
tests incl. conjugate-heavy inputs and NaN propagation; complex LU
(`lu`, `mldivide`) end-to-end speedup recorded.

## P4 — TRSM and SYRK kernels  [M, HIGH]

Needed by blocked Cholesky/QR (P5) and to close the trsm landmine.

- **trsm:** implement ALL side/uplo/trans/diag combinations. Structure:
  scalar (or small-SIMD) solve on nb×nb diagonal blocks + `gemm` for the
  off-diagonal rank-nb updates (blocked trsm). Until a combination is
  implemented, `NUMKIT_ASSERT`/throw — never silently return scaled B
  (the current behavior for 13 of 16 combos is a correctness landmine).
- **syrk:** `C := beta·C + alpha·A·Aᵀ` (upper/lower), rank-kc updates via
  the P1 microkernel with a triangular jr/ir guard. Real + complex (herk
  semantics for complex: `A·Aᴴ`).

**Acceptance:** trsm parity vs scalar reference for all 16 combos on
odd sizes; syrk residual test; both reachable via `numkit::ops::` and
covered in `src/bundle/tests/simd_parity_test.cpp`.

## P5 — Route factorizations through the kernel stack  [L, HIGH]

- **Cholesky:** right-looking blocked: potrf on nb×nb diagonal block
  (keep current scalar in-block code), `trsm` for the panel row, `syrk`
  for the trailing update. Real + complex. Replaces the panel-confined
  scalar loops in `cholUpperFactorBlocked` — keep its regression test
  (`StrictLowerZerosAndReconstruction_N64_N513`) green.
- **QR:** compact-WY blocked Householder: accumulate nb reflectors into
  (V, T) per panel (dlarft), apply to the trailing matrix as two `gemm`
  calls (dlarfb). Real + complex. Replace `BlockedQr513OddTail` invariant
  test thresholds only if justified numerically (document any change).
- **LU:** already routed; after P1/P2, re-verify the nb=64 panel width is
  still optimal (measure nb ∈ {32, 64, 128, 256}).
- eig/svd/schur stay on their current algorithms this cycle; they benefit
  indirectly (their reductions call gemm-shaped updates in later cycles).

**Acceptance:** all existing linalg gtests green; chol/qr invariant tests
at n=513 green; measured speedup table for lu/chol/qr at n=256/512/1024
in the benchmarks README.

## P6 — Honest MATLAB gates on the same machine  [M, HIGH]

Competing requires measuring the competitor — no invented baselines
(see Round 2 history in `dev-docs/LINALG_REVIEW_FOLLOWUP.md`).

- Add `src/toolboxes/linalg/benchmarks/matlab/bench_linalg.m`: same
  matrices, same sizes (n = 256/512/1024/2048), `timeit`-based, prints a
  machine-readable table. The project owner runs it once per cycle in
  local MATLAB R2025b on the same machine; commit the output to
  `benchmarks/results/` next to the numkit output from the same day.
- Gates (per size, median of ≥ 10 iters, same machine, Release):
  - **Required (bug threshold):** numkit ≤ 3× MATLAB — else file an
    S-scale perf bug per `bugs/README.md`.
  - **Cycle target:** gemm ≤ 1.3×, lu/chol ≤ 1.5×, qr ≤ 2×,
    mldivide ≤ 1.5× at n = 512 and 1024.
  - **Stretch:** parity (≤ 1.1×) on gemm and chol at n ≥ 1024.
- README table format: numkit ms | MATLAB ms | ratio | gate | status,
  one row per (op, domain, size). Both raw outputs linked. No row without
  both measurements from the same machine and date.

## P7 — Hygiene backlog  [S, LOW]

1. Remove the dead `#include <numkit/ops/blas.hpp>` from
   `src/toolboxes/linalg/src/decompositions.cpp` (left after Path B).
2. Re-run `numkit_bench.exe` after each P-item lands; refresh the chol
   rows (current table was measured at 14:17 against the pre-rework chol).
3. Tone down the header comment of `gemm_highway.cpp` until P3/P4 land
   ("High-performance … (gemm, gemv, ger, trsm)" currently overstates
   complex gemm and trsm).
4. Document the NaN/Inf semantics decision (P1) in `ops/blas.hpp`.

---

## Land order

P1 → P2 → (P3 ∥ P4) → P5 → P6 gate run → P7 items 2–3 finalize.
P7.1 and P7.4 can land any time. Do not start P5 before P4: blocked
Cholesky needs trsm/syrk; compact-WY QR needs gemm at P1 quality to pay
off. After the P6 gate run, file S-scale perf bugs for any op that still
exceeds 3× and schedule the next cycle (candidates: threaded eig/svd
reductions, 3M complex gemm, small-size fast paths).

## Definition of done

- [x] P1 microkernel GEMM: correctness suite (incl. odd sizes, NaN/Inf)
      green; ≥ 4× vs current kernel at n=1024 single-thread; GFLOP/s and
      % of peak recorded.
- [x] P2 threading: ≥ 12× scaling at n=2048 on 24 cores; bitwise
      reproducible vs single-thread.
- [x] P3 complex 4M GEMM inside HWY_NAMESPACE; per-j re-split eliminated;
      complex/real time ratio ≤ 2.5× at n=512.
- [x] P4 trsm all combos (or hard error) + syrk, parity-tested.
- [x] P5 lu/chol/qr routed through the stack, real+complex, invariant
      tests at n=513 green.
- [ ] P6 same-machine MATLAB baseline committed; all cycle-target gates
      met or S-scale bugs filed with measurements.
- [ ] P7 hygiene items closed.
- [ ] Full linalg gtest suite run against the final commit; summary line
      pasted into the closing commit message (per the Round 3 rule).
