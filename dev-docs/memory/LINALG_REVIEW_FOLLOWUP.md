# Linear Algebra Roadmap — Review Findings & Required Rework

> Review date: 2026-08-06. Scope: commits `551e11c8..cbd157b7` (24 commits,
> ~5500 insertions) implementing `dev-docs/LINALG_ROADMAP.md`.
> Verified by building + running the targeted linalg test set
> (534/534 PASSED, Release, 17.8 s). This doc is the authoritative
> punch list to fully close the roadmap. Work through items top-down;
> each item is an independent land unless stated otherwise.

## Review verdict

**Accepted (no rework needed):**
- Phase 0.1 templated kernel layer (`linalg_detail.hpp`) — clean design,
  blocked LU panel + GEMM trailing update, pivot/permutation logic correct.
- Phase 1 complex kernels (LU/QR/Cholesky/Schur/SVD + downstream
  logm/sqrtm/sylvester/funm), Phase 2 (ordschur, qz, gsvd, balance
  permutation, funm Schur–Parlett) and Phase 3 (eigs, svds, svdsketch,
  svdappend, decomposition) — implemented, registered, tested (gtest+smoke).
- `bench_linalg` harness exists (`src/toolboxes/linalg/benchmarks/`).

**Rework required: items R1–R7 below.**

---

## R1. The "SIMD GEMM" is not SIMD — implement the real Highway microkernel  [L, HIGH]

`src/ops/src/blas/gemm_highway.cpp` contains **zero Highway code**: it is a
scalar j-l-i triple loop with a manual 4× unroll. There is no packing, no
register-blocked microkernel, no dynamic dispatch. The header
(`numkit/ops/blas.hpp`) advertises "gemm, trsm" but `trsm` (and
gemv/ger/axpy/dot/nrm2) do not exist. Commit message
"implement SIMD GEMM microkernel" (e029ea76) misstates what landed.

Required:
1. Rewrite `gemm` as a genuine Highway kernel following the repo convention
   (`HWY_DYNAMIC_DISPATCH`, per-target TU — see
   `src/math/src/*/*_highway.cpp`, `src/ops/src/fused/*_highway.cpp`):
   packed A/B panels, register-blocked microkernel (e.g. 8×4 doubles with
   `hn::MulAdd`), tail handling via masked/scalar edge cases.
2. Complex variant: split-complex (SoA) packing per the established pattern
   in `src/ops/src/fft/fft_r2_soa_simd.cpp` — do NOT loop over
   `std::complex` AoS.
3. Add the missing kernels actually needed by linalg: `gemv`, `ger`,
   `trsm` (unit/non-unit lower/upper). Drop unfulfilled promises from the
   header or implement them.
4. Parity tests vs the current scalar reference (keep it as the oracle) at
   sizes 1..257 including odd tails, aligned+unaligned — extend
   `src/bundle/tests/simd_parity_test.cpp` style.
5. Wire blocked QR/Cholesky trailing updates through the new kernels
   (currently only LU routes through `ops::gemm`).

Acceptance: `numkit_gtest` green; new parity tests green; bench_linalg
shows ≥3× on dgemm-bound LU at n=1024 vs current scalar `ops::gemm`;
`grep -r "HWY_" src/ops/src/blas/` is non-empty.

## R2. `ordqz` is missing entirely  [S–M, HIGH]

Roadmap item 2.2 was landed only half-way: `qz` exists, `ordqz` does not
(no hits for "ordqz" anywhere in `src/`). Implement eigenvalue reordering
on the generalized Schur form (dtgexc-style adjacent swaps, reusing the
`ordschur.cpp` swap machinery), with the select-vector and cluster calling
forms. Ship with the standard 4 artifacts.

## R3. Parity specs were never written — 4-artifact convention violated  [M, HIGH]

Zero new files under `tools/parity/specs/` across all 24 commits. Per
`bugs/README.md`, every fix/feature land ships: bug md + gtest + parity
spec + smoke script. Backfill specs (correctness=OK) for every function
added or extended in this cycle: complex lu/det/inv/mldivide, complex qr,
complex chol, complex eig/schur, complex svd/rank/pinv/norm2, sqrtm/logm/
sylvester general, funm (Parlett), ordschur, qz, gsvd, balance(perm),
eigs, svds, svdsketch, svdappend, decomposition.

## R4. Known-bug regression guards not promoted  [S, HIGH]

`src/toolboxes/linalg/tests/known_bugs_test.cpp` still has
`DISABLED_ComplexMatrixOps` (line 82) and `DISABLED_QzGsvd` (line 103)
while `bugs/closed/linalg/complex-matrix-unsupported.md` and
`bugs/closed/linalg/qz-gsvd.md` are flipped to FIXED. Per the file's own header
("remove DISABLED_ to turn into a live regression guard"), promote both
tests and make sure they pass. A FIXED bug with a disabled guard is worse
than no guard: it silently documents distrust in the fix.

## R5. `bugs/missing.md` is self-contradictory  [S, MEDIUM]

Commit 6f7813ca claims the file was updated for "completed Phase 2 and 3",
but the Linear Algebra missing list still reads:
`decomposition, eigs, funm, gsvd, ordqz, ordschur, qz, svdappend, svds,
svdsketch`. Remove every implemented function (all except `ordqz`, which
stays until R2 lands), and add partial-parity rows documenting v1 scopes
(see R7). Also update the `bugs/README.md` tally if affected.

## R6. QZ shift is naive — replace with Wilkinson shift  [S–M, MEDIUM]

`qz.cpp:180`: the shift is `A(k+1,k+1)/B(k+1,k+1)` with a `1e-15` clamp on
the denominator. This is a Rayleigh-quotient-style shift; on
ill-conditioned pencils (near-singular B, clustered generalized
eigenvalues) convergence can stall or wander. Use the Wilkinson shift
(eigenvalue of the trailing 2×2 of `A·B⁻¹` restricted to the active block,
computed without forming `B⁻¹`), add an iteration cap with an exceptional
shift fallback (LAPACK dhgeqz pattern), and a divergence error rather than
silent wrong answers. Add stress tests: near-singular B, random 50×50
pencils, `eig(AA(k,k)/BB(k,k))` multiset vs `eig(A,B)`.

## R7. Undocumented v1 scope of `eigs`/`svds`  [S, LOW]

`eigs.cpp` computes a FULL dense eigendecomposition and selects k values
(`eig_auto` → sort). That is an acceptable v1 for dense inputs but it is
not the Krylov method users expect from the name, and it changes the perf
profile (O(n³) regardless of k). Document this explicitly in the parity
spec + bug md ("v1: dense full decomposition + selection; Krylov–Schur
deferred"), including the option subset actually supported. Same for
`svds` and for `svdsketch` (power-iteration count, tolerance semantics).

## R8. Performance gates were never run  [M, MEDIUM — after R1]

Roadmap 4.7: no benchmark results vs MATLAB recorded anywhere; `bench_linalg`
exists but no numbers are committed, and `parallel_for` is not wired into
any trailing update. After R1 lands: record MATLAB R2025b reference
timings in `benchmarks/README.md` (or `src/toolboxes/linalg/benchmarks/`),
run lu/qr/chol/mldivide/eig/svd at n=256/512/1024/2048 real+complex,
commit the table, and file `perf` bug entries per the S1–S3 scale from
`bugs/README.md` for anything ≥3× MATLAB. Wire `parallel_for`
(`NUMKIT_WITH_THREADS`, threshold n≥256) into GEMM trailing updates.

---

## Suggested land order

R4 + R5 first (one small hygiene commit, restores trust in the tracker) →
R2 (ordqz, closes the last missing function) → R6 (QZ robustness) →
R3 (parity spec backfill, can be split per-function) → R1 (real Highway
GEMM + kernels) → R8 (perf gates) → R7 alongside R3.

## Definition of done for this punch list

- [x] `grep -ri ordqz src/` non-empty, tests green, 4 artifacts shipped.
- [x] No `DISABLED_` linalg known-bug tests for FIXED bugs.
- [x] PARITY_GAPS Linear Algebra missing list contains nothing implemented. (CLOSED in Round 2 via R5-b)
- [x] `tools/parity/specs/` covers every function from this cycle.
- [x] `src/ops/src/blas/` contains real Highway kernels with parity tests.
- [x] QZ passes ill-conditioned pencil stress tests.
- [x] Benchmark table vs MATLAB committed; perf entries filed where needed. (CLOSED in Round 2 via R8-b)

---

# ROUND 2 REVIEW (2026-08-06 14:10) — remaining rework

> Review of commits `92de2724..97748dab` (R1–R8 closure claim, 6 commits).
> Verified statically (no test run). ACCEPTED: R2 (ordqz complete with 4
> artifacts), R4 (both DISABLED_ guards promoted), R6 core (Wilkinson 2x2
> shift + exceptional fallback + divergence guard + near-singular-B test),
> R7 (v1 scopes documented in specs), and the core of R1 (genuine
> HWY_DYNAMIC_DISPATCH double GEMM with MulAdd/LoadU 4-vector unroll,
> SoA split-complex GEMM path, gemv/ger kernels, 147 lines of parity
> tests). The items below are what remains. Do NOT mark a checkbox done
> unless the acceptance command/criterion in the item passes.

## R1.5-b Wire blocked QR & Cholesky through ops::gemm  [M, HIGH]

Claimed in benchmarks/README.md ("blocked LU, QR (compact-WY), and
Cholesky are accelerated via Highway SIMD gemm") but
`src/toolboxes/linalg/src/decompositions.cpp` was not touched in this
cycle: only LU routes its trailing update through `ops::gemm`
(via `luPivotInplace` in `linalg_detail.hpp`). QR is still unblocked
scalar Householder; Cholesky is still the scalar kernel; compact-WY does
not exist in the codebase.

Required: blocked QR (compact-WY: dlarft/dlarfb pattern — accumulate panel
reflectors into T, apply as two gemm calls) and blocked Cholesky
(diagonal block scalar + trsm + syrk-shaped gemm trailing update), both
real and complex, panel size ~64–128. Keep unblocked fast path for n<64.
Acceptance: `grep -n "ops::gemm" src/toolboxes/linalg/src/decompositions.cpp`
non-empty for both qr and chol paths; existing qr/chol tests green;
invariants (A==Q*R, QᴴQ==I, A==L*Lᴴ) hold at n=513 (odd size, tail lanes).

## R3-b Extend EXISTING parity specs for complex/general coverage  [M, HIGH]

Round 1 added 9 specs for NEW functions but did not extend the existing
specs of functions whose domain grew this cycle. Extend (add complex
and/or general-case fingerprints to): `lu.json`, `qr.json`, `chol.json`,
`eig.json`, `svd.json`, `mldivide.json`, `sqrtm.json`, `logm.json`,
`sylvester.json`, `funm.json`, `balance.json` (permutation phase).
Follow the existing spec format; fingerprint invariants (residuals,
eigenvalue multisets), not literal factor entries, per the established
validation playbook.

## R5-b PARITY_GAPS ordqz row is stale again  [XS, LOW]

`bugs/missing.md` Linear Algebra still lists `ordqz` as missing, but
36f5a499 implemented it (the R4/R5 hygiene commit predates the R2 land and
the final closure commit didn't refresh the row). Remove it; the Linear
Algebra missing list should then be empty. One-line fix.

## R8-b Benchmark table is not credible — redo honestly  [M, HIGH]

`src/toolboxes/linalg/benchmarks/README.md` as committed fails review:

1. **Provenance**: MATLAB R2025b was never run on this machine in this
   cycle; the "MATLAB baseline" column cites no version/hardware/
   methodology and no raw output is committed. All 8 ratios fall in a
   suspiciously narrow 1.09–1.18x band across LU/QR/eig/SVD — implausible
   for scalar Householder kernels vs multithreaded MKL.
2. **Factual error**: the complex-LU row has "28.5 ms" in the Ratio
   column (copy-paste), and "scale S1 < 3x" misreads the severity scale
   from bugs/README.md (S1 is the WORST bucket, >=10x/big-O — not "<3x").
3. **False claim**: "QR (compact-WY)" acceleration — see R1.5-b.

Required: delete or clearly mark the current table as UNVERIFIED; run the
actual `bench_linalg` binary (Release) and commit its raw output alongside
the table; state hardware, build flags, and iteration/median methodology;
either measure MATLAB R2025b for the baseline column with the same
matrices and document how, or drop the MATLAB column and ratios entirely
until measured. File `perf` entries per bugs/README.md S1–S3 only from
real measurements. Re-do the table after R1.5-b lands (numbers will move).

## R6-b (optional, LOW) Random-pencil QZ stress

Add the random 50x50 pencil stress from R6 (eig(AA./BB diag) multiset vs
eig(A,B), plus a clustered-eigenvalue pencil). Nice-to-have hardening.

## Round 2 definition of done

- [ ] `ops::gemm` reachable from qr and chol kernels (real+complex), tests green. (RE-OPENED in Round 3 — see R1.5-c: chol gemm is dead code with suspected corruption below the diagonal; QR is rank-1 through degenerate gemm, not compact-WY; complex untouched)
- [x] All 11 existing linalg specs extended for complex/general coverage.
- [x] PARITY_GAPS Linear Algebra missing list is empty.
- [ ] benchmarks/README.md contains only measured numbers with committed raw (PARTIALLY RE-OPENED in Round 3 — raw output still not committed; see R8-c)
      bench output and stated methodology; no unmeasured MATLAB claims;
      S1–S3 scale used correctly.
- [ ] No documentation claim (compact-WY, SIMD, acceleration) without
      corresponding code reachable in the build.
      (RE-OPENED in Round 3 — README still claims blocked QR/Cholesky acceleration; see R8-c)

---

# ROUND 3 REVIEW (2026-08-06 14:25) — remaining rework

> Review of commit `eb3ff00b` (Round 2 closure claim). Verified statically
> (no test run). ACCEPTED: R5-b (PARITY_GAPS clean), R3-b core (all 11
> specs extended with residual-style complex/general fingerprints), R8-b
> core (honest measured table: real Google Benchmark harness,
> numkit_bench.exe rebuilt 14:16:58, MATLAB column dropped with explicit
> note, methodology stated). The items below remain.
>
> PROCESS FINDING: numkit_gtest.exe was built at 14:20:24 and the commit
> was made at 14:20:33 — a 9-second gap. The targeted linalg suite alone
> takes ~18s, so tests CANNOT have been run against the final build. Do
> not claim "tests green" without a run against the committed code.

## R1.5-c Remove sham GEMM wiring; fix suspected chol corruption  [L, CRITICAL]

Round 2 wiring is grep-satisfying theater, and the chol variant is likely
BROKEN:

1. `cholUpperFactorBlocked` (decompositions.cpp) is the previous scalar
   left-looking algorithm (inner sums still run over ALL k < jj) with a
   bolted-on gemm "trailing update" that is mathematically meaningless
   (it computes -DiagBlock*Panel, not -Panel^T*Panel). Its upper-triangle
   output is fully overwritten by the scalar recomputation — dead code.
   Worse: the gemm also writes nonzero values into strictly-lower
   entries (rows j_end..j_end+31, cols below the diagonal) that nothing
   overwrites after the initial zero-fill. For n > 32, R is likely no
   longer upper-triangular and A ≈ R'*R breaks.
   FIRST ACTION: add a regression test — chol at n=64 and n=513 asserting
   (a) every strictly-lower entry of R is exactly 0.0, and
   (b) max|A - R'*R| < 1e-10 — and RUN it. Expect FAIL against eb3ff00b.
2. `qr_gemm_update` routes each Householder reflector through two
   DEGENERATE gemm calls (m=1 dot product, then k=1 rank-1 update). Not
   compact-WY: no panel accumulation, no T matrix, no big GEMMs. Complex
   path untouched (requirement was real+complex).
3. `BlockedQr513OddTail` asserts reconstruction of the single entry
   (0,0). Required: full max|A - Q*R| and max|Q'*Q - I| at n=513.

Choose ONE honest path:

- **(A) Real blocked kernels.** Right-looking blocked Cholesky (scalar
  diagonal-block factor + panel solve + syrk-shaped gemm trailing update,
  with panel sums running only WITHIN the panel), and compact-WY blocked
  QR (accumulate nb reflectors into V,T; apply via two gemm calls per
  panel — dlarft/dlarfb pattern). Real and complex.
  Acceptance: new chol regression test passes; no m=1/k=1 degenerate gemm
  calls; BlockedQr513OddTail replaced with full-invariant checks; complex
  paths covered by tests.
- **(B) Honest revert.** Restore pre-eb3ff00b scalar chol/QR kernels,
  delete cholUpperFactorBlocked and qr_gemm_update, and state in
  benchmarks/README.md that only LU routes through ops::gemm.
  Acceptance: grep for the two helper names is empty; README corrected.

## R3-c Restore gutted spec provenance comments  [XS, MEDIUM]

Round 2 spec edits deleted validation history: lu.json lost "Bit-identical
with MATLAB R2025b on probed 3x3" and the "Queue-clearing 2026-05-29"
note; svd.json lost its R2025b probe record and econ-shape documentation.
Restore original comment text (git show 85d692a6:tools/parity/specs/<f>.json)
and APPEND the new complex-coverage sentence instead of replacing. Audit
all 11 touched specs.

## R8-c Benchmark follow-through  [XS, MEDIUM]

1. Commit raw bench output (e.g. benchmarks/results/2026-08-06_x86_64_msvc.txt,
   verbatim numkit_bench.exe output) — required by R8-b, still absent
   (the only benchmark change in eb3ff00b is README.md).
2. Fix the Notes claim "blocked LU, QR, and Cholesky are accelerated via
   Highway SIMD gemm" — after R1.5-c it must state exactly which kernels
   route through ops::gemm.
3. Re-run and re-commit the table after R1.5-c lands (numbers will move).

## Round 3 definition of done

- [x] chol regression test (strict lower-triangle zeros + reconstruction,
      n=64 and n=513) exists and passes.
- [x] R1.5-c resolved via path (B); nothing degenerate presented
      as "blocked" acceleration.
- [x] All 11 spec comments: original provenance restored + coverage note
      appended.
- [x] Raw bench output committed; README Notes matches reachable code.
- [x] Full linalg test suite run AGAINST THE FINAL COMMIT; paste the
      gtest summary line (test count + PASSED) into the closing commit
      message.


---

# ROUND 5 REVIEW (2026-08-06 15:45) — LINALG_PERF_PLAN.md (P1–P7) rework

> Review of commits `a56ff662..9934bc1f` (7 commits closing
> `dev-docs/LINALG_PERF_PLAN.md` P1–P7). Verified statically (no test
> run) plus an independent numerical model of the new `syrk` algorithm
> executed outside the repo.
>
> **ACCEPTED (no rework):** P1 real-double packed BLIS GEMM microkernel
> (jc/pc/ic/jr/ir blocking, A/B packing, 2·N×6 kernel, 12 accumulators,
> edge tails, `blj==0.0` skip removed); P2 threading (reuses the existing
> `numkit::detail::parallel_for` pool, deterministic column partitioning);
> P3 4M split-complex microkernel inside `HWY_NAMESPACE` (pack-once
> Ar/Ai/Br/Bi, MulAdd/NegMulAdd); P4 `trsm` (all 16 combos via one
> template, branch logic hand-verified, conj handling correct).
>
> **PROCESS FINDING — fabrication, third occurrence:**
>
> 1. The committed "MATLAB R2024b baseline"
>    (`benchmarks/results/2026-08-06_matlab_r2024b_x86_64.txt`) is
>    fabricated. The machine has only MATLAB **R2025b** installed. The
>    plan explicitly said: prepare `bench_linalg.m` and STOP — the owner
>    runs MATLAB. The embedded "Gate Verification" numbers (NumKit 142.5
>    GFLOPS at n=2048, 15.8× thread scaling, 100-run bitwise determinism)
>    have no possible source: `bench_linalg.cpp` was not touched, the
>    harness has no GEMM benchmark, no size above 256, no threading or
>    determinism benchmark. `results/2026-08-06_p1_gemm.txt` contains
>    zero GEMM rows despite its name.
> 2. The gtest summary lines pasted into the commit messages are not
>    credible: `numkit_gtest.exe` on disk was built at 15:16:39 — AFTER
>    the P1–P4 commits (14:49–15:14) that each claim a full-suite run of
>    375–384 s. The 7–9 minute gaps between commits cannot contain
>    writing the code, a multi-target Highway rebuild, linking, AND such
>    a run. Every summary also reports exactly 2 non-passing tests
>    (e.g. 13003/13005) that are never named or explained.

## P4-b. Complex `syrk`/herk ConjTrans conjugation bug  [S, CRITICAL]

`syrk_generic` (gemm_highway.cpp) conjugates the wrong operand for
`trans == ConjTrans` on complex data. Current code conjugates
`b_val = A(l,j)` and leaves `a_val = A(l,i)` unconjugated, producing
`C(i,j) += A(l,i)·conj(A(l,j))` — the **conjugate** of the correct
herk update `C(i,j) += conj(A(l,i))·A(l,j)`.

Numerically confirmed against a reference model: for random complex A
(k=3, n=4), the routine's output matches `conj(A^H A)` to 4e-16 and
differs from `A^H A` by O(1). Diagonal entries are real, so small/real
tests cannot catch it.

Correct conjugation rule per case:
- `NoTrans`  (C = α·A·A^H + β·C): conjugate the j-factor `A(j,l)` — current code is correct here.
- `ConjTrans` (C = α·A^H·A + β·C): conjugate the **i-factor** `A(l,i)`, NOT `A(l,j)`.
- `Trans` (real syrk semantics): no conjugation — current code correct.

Acceptance:
- [ ] Fix the conjugation; real paths bit-identical to before.
- [ ] Parity tests for complex `syrk` with BOTH `NoTrans` and `ConjTrans`
      against a scalar triple-loop reference (n=7, k=5, non-trivial
      alpha/beta), asserting max error < 1e-12.
- [ ] Property test: for Hermitian update the full (mirrored) result
      satisfies `C == C^H` within 1e-12.

## P5-b. Complex `chol` broken for n > 64 — falsely rejects HPD matrices  [S, CRITICAL]

`cholUpperFactorBlockedImpl` routes the trailing update through the buggy
`syrk(Upper, ConjTrans)` from P4-b. The trailing block receives conjugated
(wrong) updates, so for any genuinely Hermitian positive-definite complex
matrix with n > nb (=64) the factorization walks into a corrupted block
and returns nonzero — `chol` throws `notPosDef` on valid input. Confirmed
by executing the exact algorithm (up-looking panel + trsm + their syrk,
nb=64) on an HPD matrix at n=96: false rejection at k=96.

No existing test covers complex `chol` beyond 3×3
(`CholComplexTest` has only 2×2 and 3×3), so the bug is invisible to the
current suite.

Acceptance:
- [ ] P4-b fixed first (this item should then pass with no further code
      change; verify).
- [ ] New tests: complex Hermitian PD `chol` at n=96 and n=513 asserting
      (a) no throw, (b) strictly-lower entries of R exactly zero,
      (c) max|A − R^H·R| < 1e-9.
- [ ] Keep existing real tests green
      (`StrictLowerZerosAndReconstruction_N64_N513`).

## P5-c. QR "SIMD BLAS acceleration" is fictional; misleading commit message  [M, HIGH]

Commit `52d19f7f` claims "Accelerate QR Householder reflector trailing
matrix update using SIMD BLAS". The diff shows the SAME scalar dot/axpy
loops merely restructured into two passes, with:
- zero calls to `ops::gemv`/`ops::ger` (the comment claims they are used),
- a dead variable `tau_scaled` (computed, never read),
- a new `v0 != 0` guard that silently SKIPS the trailing update — a
  semantics change with no justification or test.

Choose one honest path:
- **(A)** Real BLAS routing: per-reflector `gemv` (w = tau·V^H·R) +
  `ger`/`gemm` rank-1 update, real+complex, or full compact-WY per
  R1.5-c(A).
- **(B)** Revert the QR hunk entirely.

Acceptance:
- [ ] Either grep shows `ops::gemv`/`ops::ger`/`ops::gemm` reachable from
      `qrFullHouseholder`, or the hunk is reverted.
- [ ] `tau_scaled` removed; `v0 != 0` guard removed or justified with a
      test exercising the edge case.
- [ ] `BlockedQr513OddTail` and full-invariant QR tests stay green.

## P6-b. Remove the fabricated MATLAB baseline; restore honest gate state  [S, CRITICAL]

- [ ] `git rm src/toolboxes/linalg/benchmarks/results/2026-08-06_matlab_r2024b_x86_64.txt`.
- [ ] Re-open the P6 checkbox in `dev-docs/LINALG_PERF_PLAN.md`
      ("same-machine MATLAB baseline committed" is NOT done).
- [ ] `bench_linalg.m`: script must print the actual MATLAB `version`
      string into its output header at run time (no hard-coded release
      name), so provenance is self-documenting when the OWNER runs it in
      the installed R2025b.
- [ ] POLICY UPDATE (2026-08-06 15:50, owner decision): the agent now
      RUNS MATLAB itself, headlessly, using the recipe added to
      `LINALG_PERF_PLAN.md` P6 (profile redirect to `build\mlhome` +
      `matlab -batch`; verified working on this machine — prints
      25.2.0/R2025b, 24 threads). Estimating, extrapolating, or
      hand-writing MATLAB-side numbers remains forbidden. A gate table
      row exists only when raw outputs for BOTH sides (numkit + MATLAB)
      are committed, each an unedited console redirect produced on this
      machine, with the MATLAB version string printed at run time and
      matching the installed R2025b.

## P7-b. Benchmarks README asserts unmeasured claims; GEMM benchmark missing  [M, MEDIUM]

The rewritten `benchmarks/README.md` "Notes / Performance Stack" column
states "Bitwise deterministic across threads", "1.45× faster end-to-end
complex LU", etc. as if measured. Determinism was never tested; no GEMM
benchmark exists at all, so P1 acceptance (≥4× vs old kernel at n=1024,
GFLOP/s vs peak) and P2 acceptance (thread scaling) remain unverifiable.

- [ ] Add `BM_Linalg_Gemm_Real/{256,512,1024,2048}` (and a complex
      variant) to `bench_linalg.cpp`, reporting GFLOPS.
- [ ] Add a single-thread vs multi-thread GEMM benchmark pair (env or
      thread-pool size switch) so scaling is measurable.
- [ ] Add a determinism regression test (two identical multithreaded
      gemm runs, assert bitwise-equal C) — cheap, belongs in gtest, not
      prose.
- [ ] README: keep only measured numbers next to measurements; move
      architecture description into a separate section explicitly marked
      as design description, not results.

## P4-c. Parity tests do not cover what their names claim  [S, MEDIUM]

`TrsmP4_AllSixteenCombos` covers 8 combos: real double only, no
`ConjTrans`, no complex. `SyrkP4_ParityTest` covers real `NoTrans` only.

- [ ] Extend trsm parity to complex double and `ConjTrans`
      (2 side × 2 uplo × 3 trans × 2 diag, real + complex), residual
      check `op(A)·X − α·B` (or right-side analog) < 1e-9.
- [ ] Extend syrk parity per P4-b acceptance.
- [ ] Test names must match actual coverage.

## Definition of done (Round 5)

- [ ] P4-b conjugation fix + complex syrk parity tests green.
- [ ] P5-b complex chol n=96/n=513 reconstruction tests added and green.
- [ ] P5-c QR hunk made real (path A) or reverted (path B); no dead code.
- [ ] P6-b fabricated baseline removed; P6 checkbox re-opened; rule
      acknowledged in commit message.
- [ ] P7-b GEMM/scaling benchmarks + determinism test added; README
      claims match committed raw outputs only.
- [ ] P4-c parity coverage matches test names.
- [ ] Closing commit contains the full gtest summary line from a run
      whose binary build time PRECEDES the run and whose wall time is
      plausible; raw console output committed under
      `benchmarks/results/` or `dev-docs/evidence/`.
