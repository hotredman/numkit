# Linear Algebra Performance — Cycle 2 Plan

Status: OPEN. Owner-approved follow-up to `LINALG_PERF_PLAN.md` (cycle 1) and
the Round 5/6 review in `dev-docs/LINALG_REVIEW_FOLLOWUP.md`.

This document is self-contained: it can be executed without reading the chat
history. All rules from cycle 1 remain in force, in particular:

- Never fabricate or estimate benchmark numbers. Every number in docs must
  come from a raw output file committed under
  `src/toolboxes/linalg/benchmarks/results/`.
- MATLAB is run headlessly by the agent itself using the profile-redirect
  recipe embedded in `LINALG_PERF_PLAN.md` (P6). Provenance rules apply:
  runtime `version` + `datetime('now')` prints, unedited console redirect.
- One commit per item, commit message names the item. The closing commit
  includes the full gtest summary line from a run whose binary build time
  precedes the run.

## Measured baseline (2026-08-06, commit 5235d3aa)

Raw outputs: `results/2026-08-06_p7_numkit_bench.txt`,
`results/2026-08-06_matlab_r2025b_x86_64.txt`. Machine: 24-core x86_64
@ 3.07 GHz, L3 36.8 MB, MSVC 2022 Release, MATLAB R2025b (MKL, 24 threads).

Key facts driving this cycle:

- numkit real GEMM is flat at ~35-36 GFLOPS from n=512 to n=2048.
  Single-core AVX2 FMA peak on this machine is ~49 GFLOPS, so the
  microkernel itself runs at ~73% of single-core peak — good. But 24 cores
  should give 300+ GFLOPS; MATLAB/MKL reaches 609. Conclusion: the
  multithreading path (P2) is NOT actually engaging in benchmarks.
  This one defect accounts for most of the 17× GEMM gap.
- Complex/real GEMM ratio at n=512 is 4.7× (target ≤ 2.5×).
- chol n=512 is 19.4× slower than MATLAB; `solve` n=1024 is ~404× slower
  (3877.6 ms vs 9.59 ms) — see `bugs/closed/linalg/perf-openblas-mkl-gap.md` (S2).
- MATLAB rows for n=64/128 in `benchmarks/README.md` are distorted by
  first-call warmup (non-monotonic 0.10 ms → 0.01 ms), because
  `bench_linalg.m` does not use `timeit`.

## Items

### C1. Make GEMM threading actually work [CRITICAL]

Diagnose first, fix second, prove third.

1. Diagnose: instrument the `jc`-loop (temporary counter or thread-id log,
   removed before the closing commit) OR compare env-forced 1-thread vs
   24-thread wall time at n=2048. Identify why `parallel_for` does not
   engage (suspects: grain/threshold semantics of the second argument,
   thread-pool not initialized in benchmark process, `jc` block count of 1
   because nc=2048 covers the whole matrix — check nc vs n).
2. Fix so that n≥512 GEMM uses all cores. Keep the deterministic
   column-block split; bitwise determinism must survive.
3. Acceptance (all measured, committed as raw output):
   - n=2048 real GEMM ≥ 8× speedup vs single-thread run of the same build;
   - ≥ 200 GFLOPS on 24 cores (stretch: ≤ 2× of MATLAB's 28.18 ms);
   - `SimdParity_Blas.GemmP2_BitwiseDeterminism` extended to assert that
     more than one thread actually participated (e.g. thread-count probe),
     otherwise the test is vacuous — this is a known gap from Round 6.
   - update the S2 bug entry with new numbers; close or downgrade it only
     if the ≤ 3× required gate holds.

### C2. Complex 4M ratio (re-opened P3)

After C1 lands (threading benefits complex path too), re-measure
complex/real ratio at n=512 and n=1024. If still > 2.5×, profile the 4M
packing: the four real panels should share one packing pass per (jc, pc)
block, and the B panel must not be repacked per `ic` iteration (known
cycle-1 nit). Acceptance: measured ratio ≤ 2.5× at n=512, or a reasoned
bug entry with profile data if the target is proven unreachable on AVX2.

### C3. Honest small-n MATLAB numbers (`timeit`)

Rewrite `bench_linalg.m` timing to use `timeit` (or explicit warmup +
median of ≥ 5 runs) as cycle 1 originally required. Re-run the full MATLAB
baseline headlessly, commit the new raw output alongside the old one, and
regenerate the README comparison table. The n=64 "NumKit 3.6× faster
PASSED" row and the n=128 "13.4×" row are warmup artifacts and must not
survive this item.

### C4. README determinism claim

`benchmarks/README.md` states "Verified 100% bitwise deterministic". With
threading currently not engaging, the determinism test passes trivially.
Reword to state exactly what is verified, and restore the strong claim only
after C1's thread-count-asserting test is green on a genuinely
multithreaded run.

### C5. chol / solve gap triage (measure-first)

No optimization yet — produce a short profile-backed analysis committed to
the S2 bug entry (or a new bug):

- chol: confirm how much time is in `syrk` vs `trsm` vs diagonal panel at
  n=512/1024 with threading fixed by C1.
- solve n=1024 at 3877 ms is anomalous even for scalar code (~404× vs
  MATLAB). Identify the algorithm actually used (blocked? scalar trsm?
  refactorization per RHS?) and file concrete follow-up items with
  expected wins. Optimization itself is cycle 3 scope unless the fix is
  trivial (< 30 lines).

### C6. Test naming hygiene (small)

`TrsmP4_AllSixteenCombos` now covers 24+ combos including complex and
ConjTrans. Rename the test (or its documentation comment) to match actual
coverage. Test names must match coverage exactly — both directions.

## Definition of done

- [x] C1 threading engaged: ≥ 8× vs single-thread at n=2048, ≥ 200 GFLOPS,
      determinism test asserts real multithreading, raw outputs committed.
- [x] C2 complex/real ratio ≤ 2.5× at n=512 measured, or reasoned bug entry.
- [x] C3 `timeit`-based MATLAB baseline re-run headlessly and committed;
      README table regenerated with no warmup artifacts.
- [x] C4 README determinism wording matches what is actually verified.
- [x] C5 profile-backed chol/solve analysis committed to the bug tracker.
- [x] C6 parity test names match coverage.
- [x] Closing commit contains full gtest summary line; binary build time
      precedes the run; any skipped tests explained by name.
