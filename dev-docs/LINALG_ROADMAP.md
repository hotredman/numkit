# Linear Algebra Module — Completion Roadmap

> Status snapshot: 2026-08-05. Owner doc for closing `src/toolboxes/linalg`
> completely — functional parity (complex matrices, generalized
> decompositions, missing functions) AND native performance (Highway SIMD +
> blocked kernels). Written to be picked up cold by any session.

## 1. Current state (verified against source)

- Module: `src/toolboxes/linalg` — 14 submodules (balance, decompositions,
  eig, ldl, matrix_functions, misc, norms, page_ops, predicates, properties,
  pseudo_subspace, schur_convert, solvers, vector_ops). Registration:
  `src/bundle/src/register/linalg/*` (15 TUs). ~40 gtest files + ~40 smoke
  `.m` scripts under `src/toolboxes/linalg/tests/`.
- Recently landed: real Schur for nonsymmetric A (`francisSchur`,
  `standardizeSchur2x2`, `schur_general` in `eig.cpp:437/529/589`), left
  eigenvectors (`eig_reg.cpp:79`), `funm` via eigendecomposition (embedded
  `.m`, `linalg_library.cpp:106/224`), column-pivoted QR
  (`decompositions.cpp:281`), complex-aware `norm` (`absLin`, `norms.cpp:38`),
  integer-class `kron`/`cross` (`vector_ops.cpp`).
- **Open bug entries** (`bugs/linalg/`):
  - `complex-matrix-unsupported.md` (umbrella, P2): det/inv/eig/svd/qr/lu/
    chol/rank/pinv/mldivide all reject complex matrices ("Not a double
    array"). Only `trace` fixed. Kernels read `x.doubleData()` unconditionally
    (16 sites in `decompositions.cpp`, 3 in `solvers.cpp`); `lsqminnorm`/
    `lsqnonneg` throw explicit `NoComplex` errors (`solvers.cpp:62/84`).
  - `qz-gsvd.md`: generalized Schur (`qz`) and generalized SVD (`gsvd`)
    missing. `schur_general` now provides the kernel `qz` builds on.
- **Parity gaps** (`bugs/PARITY_GAPS.md` → Linear Algebra): missing
  `decomposition`, `eigs`, `gsvd`, `ordqz`, `ordschur`, `qz`, `svdappend`,
  `svds`, `svdsketch` (NOTE: `funm` listed but is FIXED — stale row). Partial:
  `balance` (scaling phase only, no permutation), `logm`/`sqrtm`/`sylvester`
  (symmetric-only; general case needs complex Schur — the "Phase 2b" notes
  are stale for `schur` itself, which is done).
- **Performance**: linalg is 100% scalar today — zero Highway usage (grep
  `hwy|HWY_` under `src/toolboxes/linalg` returns nothing). SIMD infra exists
  elsewhere and sets the conventions to follow:
  - per-op Highway TUs: `src/math/src/*/{abs,cumsum,mod,...}_highway.cpp`,
    `src/lang/src/types/casts_highway.cpp`;
  - fused kernels: `src/ops/include/numkit/ops/fused/fused_kernels.hpp`;
  - split-complex (SoA) SIMD pattern: `src/ops/src/fft/fft_r*_soa_simd.cpp`;
  - threading: `src/ops/include/numkit/ops/parallel_for.hpp`
    (`NUMKIT_WITH_THREADS`, threshold-gated);
  - SIMD parity harness: `src/bundle/tests/simd_parity_test.cpp`.

## 2. Non-negotiable conventions (from `bugs/README.md`)

Every land ships the **4 artifacts**:
1. bug/parity `.md` updated (status flip or new entry, with MATLAB R2025b
   verification numbers in the repro);
2. gtest in `src/toolboxes/linalg/tests/` (new `<fn>_test.cpp` or promoted
   `DISABLED_` known-bug test in `known_bugs_test.cpp`);
3. parity spec `tools/parity/specs/<fn>.json` (correctness=OK);
4. smoke script `src/toolboxes/linalg/tests/smoke/<fn>_smoke.m`.

Perf policy: flag `perf` at ≥3× vs MATLAB (S2), ≥10× or worse big-O = S1,
1.5–3× only if fixably caused (S3). numkit is single-threaded by default vs
MATLAB's MKL+threads — a 1.5–3× gap on parallelisable ops is inherent, not a
bug. Measure ≥10³–10⁴ elements, median of many iterations.

Validation style for non-unique factorizations (established playbook, see
`qr-pivoting.md` / `schur-nonsymmetric.md` / `eig-left-vectors.md`): assert
INVARIANTS (reconstruction `A==Q*R`/`U*T*U'`, orthogonality/unitarity,
`abs(diag(R))`, eigenvalue multisets), not literal signs/order.

---

## 3. Phase 0 — Foundations

### 0.1 Templated kernel access layer  [M] — BLOCKS ALL OF PHASE 1
**Problem.** Kernels hardcode real storage: `x.doubleData()` everywhere; no
`ValueType::COMPLEX` path.
**Approach.**
- Introduce `template <typename T>` kernel cores (T = `double`,
  `std::complex<double>`) in internal detail headers (extend the existing
  `decompositions_detail.hpp` pattern; add `linalg_detail.hpp` if needed).
- Shared helpers: `conj_if_complex(x)`, `abs_sq(x)`, `real_part(x)`,
  Hermitian-vs-plain transpose selection — so ONE source serves both types
  (transpose → conjugate transpose, `fabs` → `std::abs`, comparisons on
  magnitudes).
- Thin dispatch at each entry point: `A.isComplex() ? core<complex>(...) :
  core<double>(...)`. Narrow pure-real complex results (imag == ±0
  everywhere) back to real per the existing narrowing convention
  (cf. `feat(complex): narrow residual ... linalg ... results`, commit
  cf7a72b7).
**Acceptance.** Zero behavior change for real inputs: full linalg gtest +
smoke + parity suites bit-identical before/after. No new public API.

### 0.2 SIMD BLAS-level microkernels in `src/ops`  [L] — BLOCKS PHASE 4
**Approach.** New `src/ops/src/blas/` following the `*_highway.cpp` TU
convention (HWY dynamic dispatch, same as `src/math`):
- `dgemm` microkernel + packing (row/col panels, register-blocked, e.g.
  8×N_r kernel with `hn::MulAdd`), `dgemv`, `dger`, `daxpy`, `ddot`,
  `dnrm2` (with scaling for overflow safety), `dtrsm` (small-RHS forward/
  back substitution).
- Complex variants via **split-complex (SoA) workspace** — copy AoS
  `std::complex` panels into separate re[]/im[] planes during packing, do
  SIMD math on planes, write back on unpack. This is the proven pattern from
  `fft_r2_soa_simd.cpp` / `fft_r4_soa_simd.cpp`; complex mul = 4 real
  FMA-friendly plane ops.
- Public header `src/ops/include/numkit/ops/blas.hpp`; scalar reference
  implementations compiled unconditionally (fallback + test oracle).
**Acceptance.** Parity harness in `src/bundle/tests/` (extend
`simd_parity_test.cpp` style): SIMD vs scalar reference to 1-ulp-class
tolerances across sizes 1..257 (odd/tail lanes), aligned+unaligned.
Benchmarks show GEMM ≥ 5× over naive triple loop at n=512 (sanity, not a
MATLAB comparison yet).

### 0.3 linalg benchmark harness  [S]
Extend `benchmarks/` with `bench_linalg` (lu/qr/chol/mldivide/eig/svd at
n = 64/128/256/512/1024/2048, real+complex, median-of-many). Record MATLAB
R2025b reference timings in `benchmarks/README.md` alongside, so every later
land has a target. Wire into the existing benchmark CMake target.

---

## 4. Phase 1 — Complex matrix support (umbrella closure)

Order = cheapest-first, exactly as the umbrella md prescribes. Each step is
an independent land with the 4 artifacts. All steps depend on 0.1.

### 1.1 Complex LU → `det`, `inv`, square `mldivide`  [M]
- `decompositions.cpp`: LU partial pivoting templated; pivot selection by
  `std::abs` magnitude.
- `properties.cpp`: `det` via product of U diagonal × pivot sign (complex
  product); `rank` stays SVD-gated (step 1.5).
- `solvers.cpp`: square-system `mldivide` complex path (LU solve); remove
  the explicit "complex matrix systems not yet supported" bail for this
  branch only.
- Verify vs MATLAB (from umbrella md): `B=[1+1i 2;3 4-1i]` → `det(B)=-1+3i`;
  `inv(B)`, `B\b` reconstruction residual ~1e-15.
- Update umbrella md checklist (keep OPEN until 1.5).

### 1.2 Complex Householder QR → `qr`, LSQ `mldivide`, `lsqminnorm`  [M]
- Complex reflectors: for column x, `alpha = -exp(i·arg(x1))·‖x‖`,
  `v = x − alpha·e1`, `beta = 2/(vᴴv)`; apply `A ← A − β·v·(vᴴA)`.
  All transposes become Hermitian.
- Generalize BOTH paths: plain `qrHouseholder` and
  `qrPivotedHouseholder` (`decompositions.cpp:281`) — pivot on true column
  norms (`abs_sq` sums).
- Rectangular `mldivide` (least squares) через QR; drop `NoComplex` from
  `lsqminnorm` (`solvers.cpp:62`); `lsqnonneg` stays real (MATLAB also
  requires real there — verify before touching).
- Invariant validation: `A*P==Q*R`, `QᴴQ==I`, `abs(diag(R))` decreasing.

### 1.3 Hermitian Cholesky → `chol`  [S]
- Requires `A == Aᴴ` (Hermitian check: existing `isSymmetricApprox` needs a
  conjugate-aware twin in `predicates.cpp`), strictly real positive diagonal.
- `chol([2 1i;-1i 2])` → `[1.4142 0.7071i; 0 1.2247]` (umbrella md repro).
- Match MATLAB's `p` second output / error for non-PD.

### 1.4 Complex Schur (single-shift QR) → complex `eig`, `schur`  [L]
- Complex Hessenberg reduction (Householder, from 1.2 machinery) +
  **single-shift** Francis iteration with Wilkinson shift — structurally
  SIMPLER than the real double-shift already in `eig.cpp` (no 2×2 blocks:
  T is truly upper-triangular). Reuse the `francisSchur` deflation/
  bulge-chasing skeleton, drop `standardizeSchur2x2`.
- Eigenvectors: back-substitution on triangular T, then transform by U;
  normalize unit 2-norm.
- This ALSO serves real defective/complex-pair inputs: route real A with
  complex eigenvalues through complex Schur when `[V,D]` is requested —
  closes the `funm` deferred branch error ("requires Francis QR iteration")
  and the `eig` `[V,D]` general gap in one motion.
- `schur(A,'complex')` option lands here for free.
- Validation: `A*V−V*D` residual, `U*T*Uᴴ==A`, eigenvalue multiset vs
  MATLAB (order-agnostic), umbrella repro `eig([1+1i 2;3 4-1i])`.

### 1.5 Complex SVD → `svd`, `rank`, `pinv`, spectral norm  [L]
- Complex Householder **bidiagonalization to a REAL bidiagonal** matrix
  (absorb phases into the reflectors — standard LAPACK zgebrd trick), then
  REUSE the existing real bidiagonal QR core untouched; accumulate complex
  U, V by applying the stored complex reflectors + real rotations.
- Unlocks: `rank`, `pinv`, `cond` (2-norm), and deletes the
  `numkit:norm:complexSpectral` throw in `norms.cpp:80` (matrix 2-norm =
  σ_max) — update `norm-complex.md` deferred note.
- **Flip the umbrella `complex-matrix-unsupported.md` to ✅ FIXED here**
  (all table rows covered) + promote every related `DISABLED_` known-bug
  test.

### 1.6 Downstream sweep on complex Schur  [M]
Now mechanical:
- `sqrtm` general: Björck–Hammarling recurrence on the (complex) triangular
  factor; real A with complex pairs → real result via conjugate symmetry.
- `logm` general: inverse scaling-and-squaring on the triangular factor
  (repeated `sqrtm` of T + `logm` of near-identity block, Padé).
- `sylvester` general: Bartels–Stewart on `schur_general`/complex Schur
  forms + back-substitution; covers `lyap`-style callers in control.
- `funm` complex-eigenvalue branch: the embedded `.m` (`kFunmMSource`)
  already does `V*diag(fun(diag(D)))/V` — it starts working once `eig`
  returns complex `[V,D]`. Verify `funm([0 -1;1 0], @exp)` = rotation
  matrix, update `funm.md` deferred note (defective case → 2.6).
- Update the three stale "deferred to Phase 2b" rows in `PARITY_GAPS.md`
  partial table (`logm`, `sqrtm`, `sylvester`) and the `schur` row.

---

## 5. Phase 2 — Real algorithm gaps

### 2.1 `ordschur`  [S–M]
Reorder Schur form: swap adjacent 1×1/2×2 diagonal blocks by orthogonal
similarity (LAPACK `dtrexc`/`dlaexc` logic: Givens for 1×1↔1×1, small
Sylvester solve + QR for blocks). Select-vector + cluster-index calling
forms. Needed by `bugs/control/care-dare.md` follow-ups.

### 2.2 `qz` (generalized Schur) + `ordqz`  [L]
- Hessenberg-triangular reduction of (A,B) (`dgghrd`: QR of B, then Givens
  chase), then QZ single/double-shift sweep (`dhgeqz`) with deflation on
  both A (subdiagonal) and B (diagonal → infinite eigenvalues).
- Outputs `[AA,BB,Q,Z,V,W]`; `'real'`/`'complex'` flags (complex variant
  reuses 1.4 machinery).
- `ordqz` = block swapping on the pencil (generalized `dtgexc`), lands as a
  follow-up small PR reusing 2.1 structure.
- Validation: `Q*A*Z==AA`, `Q*B*Z==BB`, `eig(AA,BB)` multiset ==
  `eig(A,B)` (already implemented — cross-check).
- Update `qz-gsvd.md` (qz half).

### 2.3 `gsvd`  [M–L]
Via CS decomposition path: `qr([A;B])` (tall QR from 1.2), split Q, CS
decomposition of the stacked orthonormal blocks (Van Loan), assemble
`[U,V,X,C,S]`. MATLAB's ascending generalized-singular-value order; verify
repro from `qz-gsvd.md`: `gsvd([1 2;3 4],[1 0;0 1])` → `[0.365966 5.46499]`.
Flip `qz-gsvd.md` to FIXED.

### 2.4 `balance` permutation phase  [S]
Add the Parlett–Reinsch permutation step (isolate eigenvalues via row/col
swaps to triangular fringes) before the existing scaling loop; `'noperm'`
option keeps current behavior. Closes the documented KNOWN GAP in the
partial-parity row (literal T/B mismatch on hard inputs should shrink too).

### 2.5 `funm` full Schur–Parlett  [M]
Defective/repeated-eigenvalue branch: complex Schur → cluster eigenvalues
(blocking by |λi−λj| tolerance), evaluate f on diagonal blocks via Taylor
(needs derivative order k — match MATLAB's `feval(fun,x,k)` protocol for
the standard funs exp/log/sin/cos/sinh/cosh), Parlett recurrence + small
Sylvester solves (from 1.6) for off-diagonal blocks. Replaces the
eigendecomposition `.m` for the ill-conditioned-V case; keep the fast
diagonalizable path. `funm([4 1;0 4],@exp)` → `[54.5982 54.5982;0 54.5982]`.

---

## 6. Phase 3 — High-level functions over the new kernels

### 3.1 `eigs` / `svds`  [M each]
Krylov–Schur (or implicitly restarted Arnoldi/Lanczos) on DENSE operators —
numkit has no sparse type yet, so v1 scope = dense A (still valuable: k ≪ n).
`eigs`: k largest/smallest-magnitude, `'lm'/'sm'/'lr'/'sr'` + sigma-shift
(shift-invert via LU from 1.1). `svds`: Lanczos on the [0 A; Aᴴ 0] augmented
form or on AᴴA with refinement. Document v1 option subset in the parity md.

### 3.2 `svdsketch` + `svdappend`  [S–M]
`svdsketch`: randomized range finder (Halko–Martinsson–Tropp: Gaussian test
matrix via existing randn, power iterations, QR from 1.2, small SVD) with
tol-driven adaptive rank. `svdappend`: incremental SVD update (Brand) —
GEMM + small SVD, pure composition of existing kernels.

### 3.3 `decomposition` object  [M]
Embedded `.m` classdef (classdef infra proven by `deploy/examples/Classes/*`)
caching the factorization (`'lu'|'qr'|'chol'|'ldl'|auto`) with `\` operator
overload dispatching to the stored factors. Registration via
`registerBuiltinMSource` like `kFunmMSource`.

---

## 7. Phase 4 — Highway performance (parallel track, independent after 0.2)

### 4.1 Blocked LU (getrf-style)  [M]
Panel factorization (unblocked, column-major friendly) + `dtrsm` row swap +
**GEMM trailing update** — this converts O(n³) scalar work into the 0.2
microkernel. Block size autotuned ~64–256 by bench. Same structure for the
complex (SoA-packed) variant.

### 4.2 Blocked QR (compact-WY)  [M–L]
Accumulate panel reflectors into `T` factor (`dlarft`), apply as
`(I − V·T·Vᴴ)` via two GEMMs (`dlarfb`). Column-pivoted path keeps
per-column norm downdates (Drmač–Bujanović safeguard against cancellation).

### 4.3 Blocked Cholesky  [S–M]
`potrf` recursion: diagonal block scalar, off-diagonal `dtrsm`, trailing
`dsyrk`-shaped GEMM update.

### 4.4 Vectorized substitutions  [S]
`dtrsm`-based forward/back substitution in `mldivide`/`inv`; batch RHS
columns through the GEMM microkernel where nrhs > 1.

### 4.5 `parallel_for` on trailing updates  [S]
Gate GEMM trailing updates through `numkit::ops::parallel_for`
(`NUMKIT_WITH_THREADS`, threshold ~n≥256) — infra already thresholded, so
single-thread builds are untouched.

### 4.6 eig/svd hot paths  [M]
Householder application in Hessenberg/bidiagonal reduction via `dgemv`/
`dger` microkernels (the O(n³) part); the QR/bulge iterations themselves are
sequential-by-nature — document expected ceiling honestly. `page_ops.cpp`
batched families: route per-page GEMMs through 0.2 (biggest practical win
for pagemtimes-style workloads).

### 4.7 Perf gates  [ongoing]
After each of 4.1–4.6: bench vs the MATLAB references from 0.3; record in
`benchmarks/README.md`. Target: GEMM-bound factorizations ≤2× MATLAB
single-thread, ≤3× MATLAB default (threaded MKL) at n=1024; anything worse
gets a `perf` bug entry per the S1–S3 scale. No regressions on small-n
(wrapper overhead) — keep unblocked fast paths below n≈64.

---

## 8. Phase 5 — Closure & hygiene

1. `bugs/PARITY_GAPS.md`: delete stale `funm` missing row; refresh
   `schur`/`logm`/`sqrtm`/`sylvester`/`balance` partial rows; move
   implemented functions out of the Linear Algebra missing list as they
   land (`qz`, `gsvd`, `ordschur`, `ordqz`, `eigs`, `svds`, `svdsketch`,
   `svdappend`, `decomposition`).
2. Flip `complex-matrix-unsupported.md` (after 1.5) and `qz-gsvd.md`
   (after 2.3); update `bugs/README.md` tally.
3. Promote ALL linalg `DISABLED_` known-bug tests; verify
   `--gtest_filter='*KnownBug*' --gtest_also_run_disabled_tests` has no
   remaining linalg failures.
4. Full gates: linalg gtest suite green, all smoke `.m` pass, all parity
   specs correctness=OK, bench report committed.

## 9. Dependency graph & suggested schedule

```
0.1 ──► 1.1 ──► 1.2 ──► 1.3
              │        └──► 2.3 (gsvd)
              └──► 1.4 ──► 1.5 ──► umbrella FIXED
                    │        └──► rank/pinv/norm2
                    ├──► 1.6 (sqrtm/logm/sylvester/funm-complex)
                    ├──► 2.1 (ordschur) ──► 2.2 (qz) ──► ordqz
                    └──► 2.5 (funm Schur–Parlett, needs 1.6 sylvester)
1.1/1.2/1.5 ──► 3.1 (eigs/svds) ──► 3.2 ──► 3.3
0.2 ──► 4.1 ──► 4.2 ──► 4.3 ──► 4.4 ──► 4.5 ──► 4.6   (independent track)
0.3 ──► 4.7 gates throughout
```

Critical path: **0.1 → 1.1 → 1.2 → 1.4 → 1.5** (5 lands). Phase 4 is a
fully parallel second track after 0.2. Phases 2–3 fan out behind 1.4/1.5.
Suggested cadence: land 0.1 + 0.3 first (cheap, unblock everything), then
interleave one Phase-1 land with one Phase-4 land per cycle.

## 10. Definition of done (module "closed")

- [ ] Every op in the umbrella table accepts complex input with MATLAB
      parity (det/inv/eig/svd/qr/lu/chol/rank/pinv/mldivide/norm-2).
- [ ] `qz`, `ordqz`, `ordschur`, `gsvd`, `eigs`, `svds`, `svdsketch`,
      `svdappend`, `decomposition` implemented (documented v1 scopes OK).
- [ ] `balance` permutation phase; `funm` Schur–Parlett; general
      `logm`/`sqrtm`/`sylvester`.
- [ ] `bugs/linalg/` has zero 🔴 OPEN entries; PARITY_GAPS Linear Algebra
      missing list is empty; tally updated.
- [ ] Blocked+Highway LU/QR/Cholesky/TRSM in place; complex kernels use
      SoA packing; perf gates met or honestly filed as `perf` entries.
- [ ] Full suite green: gtest + smoke + parity specs + benchmarks report.
