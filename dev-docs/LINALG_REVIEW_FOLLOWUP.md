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
while `bugs/linalg/complex-matrix-unsupported.md` and
`bugs/linalg/qz-gsvd.md` are flipped to FIXED. Per the file's own header
("remove DISABLED_ to turn into a live regression guard"), promote both
tests and make sure they pass. A FIXED bug with a disabled guard is worse
than no guard: it silently documents distrust in the fix.

## R5. `bugs/PARITY_GAPS.md` is self-contradictory  [S, MEDIUM]

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
- [ ] PARITY_GAPS Linear Algebra missing list contains nothing implemented. (RE-OPENED in Round 2: ordqz row went stale again — see R5-b)
- [x] `tools/parity/specs/` covers every function from this cycle.
- [x] `src/ops/src/blas/` contains real Highway kernels with parity tests.
- [x] QZ passes ill-conditioned pencil stress tests.
- [ ] Benchmark table vs MATLAB committed; perf entries filed where needed. (RE-OPENED in Round 2: committed table is not credible — see R8-b)

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

`bugs/PARITY_GAPS.md` Linear Algebra still lists `ordqz` as missing, but
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

- [ ] `ops::gemm` reachable from qr and chol kernels (real+complex), tests green.
- [ ] All 11 existing linalg specs extended for complex/general coverage.
- [ ] PARITY_GAPS Linear Algebra missing list is empty.
- [ ] benchmarks/README.md contains only measured numbers with committed raw
      bench output and stated methodology; no unmeasured MATLAB claims;
      S1–S3 scale used correctly.
- [ ] No documentation claim (compact-WY, SIMD, acceleration) without
      corresponding code reachable in the build.
