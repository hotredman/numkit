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

- [ ] `grep -ri ordqz src/` non-empty, tests green, 4 artifacts shipped.
- [ ] No `DISABLED_` linalg known-bug tests for FIXED bugs.
- [ ] PARITY_GAPS Linear Algebra missing list contains nothing implemented.
- [ ] `tools/parity/specs/` covers every function from this cycle.
- [ ] `src/ops/src/blas/` contains real Highway kernels with parity tests.
- [ ] QZ passes ill-conditioned pencil stress tests.
- [ ] Benchmark table vs MATLAB committed; perf entries filed where needed.
