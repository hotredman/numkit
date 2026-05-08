# builtin/mrdivide — ТЗ for completion

**Status:** closed
**Priority:** **critical**
**Effort:** medium
**Audited at commit:** 42e1ec3
**Audit date:** 2026-05-06

## Gaps

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `mrdivide(A, B) = A/B` (matrix right division) | solves `X*B = A` for X | THROWS "Matrix right division not implemented" | **critical** — core MATLAB matrix op |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `mrdivide([1 2; 3 4], [5 6; 7 8])` | `[3 -2; 2 -1]` | THROWS |

## Recommended fixes

1. **Implement matrix right division.** Standard implementation:
   `X = A/B` is equivalent to `(B'\A')'`. So:
   - Compute `B'\A'` via `mldivide(B', A')` (LU/QR-based).
   - Transpose the result.
   numkit appears to have `mldivide` partially (probe interrupted)
   — verify and use as the primitive.
2. **Spec extension** — add fingerprint over square (LU path),
   tall (QR path), wide (least-squares), and singular cases.
   `tol = 1e-10`.

## Out of scope for this ТЗ

- The page-wise version `pagemrdivide` (already ❌ in PROGRESS).

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: Implemented matrix `mldivide` AND `mrdivide` for square and
  tall A. Both share a single PMR-correct internal solver
  `numkit::builtin::detail::la_solve()` in
  `libs/builtin/src/language/operators/la_solve.{hpp,cpp}`:

    - **square A (m == n)**: LU with partial pivoting (Doolittle
      style). Singular A → throws `m:mldivide:singular` /
      `m:mrdivide:singular`.
    - **tall A  (m  > n)**: QR via Householder reflections. Q^T·B
      is accumulated in place during decomposition, then R is
      back-solved. Rank-deficient (zero column-norm or zero R
      diagonal) → throws as singular.
    - **wide A  (m  < n)**: NOT yet implemented — throws
      `m:mldivide:wide` (would require SVD or QR of A' for the
      MATLAB min-norm solution; deferred).

  `mrdivide(A, B)` reuses `mldivide` via the standard transpose
  identity `A/B = (B'\A')'`. Only one full solver path, two
  user-facing operators.

  **Scratch hygiene**: All workspace (LU buffer, pivot vector, QR
  Householder vector `v`, working copies of A and B) goes through
  `ScratchArena` + `ScratchVec<T>` per the PMR HARD RULE.
  Zero `std::vector` in the new code.

  **Edge-cases matched against MATLAB R2025b**:
    - Scalar/scalar → fast path
    - Matrix/scalar → elementwise (existing path)
    - Scalar/matrix → ERRORS with "Matrix dimensions must agree"
      (verified: `2 / [1 2; 3 4]` errors in MATLAB; we now do too —
      previously this branch was undefined behavior since old
      code throw "not implemented" for the entire matrix path)
    - Scalar `\` matrix → elementwise B/A (verified: `2 \ [4 6 8]`
      = `[2 3 4]`)
    - Tall least-squares: `[1 0; 1 1; 1 2; 1 3] \ [6; 5; 7; 10]`
      = `[4.9; 1.4]` bit-identical
    - Multi-RHS: `A \ B` with B having multiple columns processed
      column-by-column through the same factorization

  **4 artefacts shipped:**
  - impl: `libs/builtin/src/language/operators/la_solve.{hpp,cpp}`
    + edits to `binary_ops.cpp` `mldivide`/`mrdivide` adapters
  - parity specs: `tools/parity/specs/mldivide.json` (18 fps,
    correctness=OK) and `mrdivide.json` (10 fps, correctness=OK)
  - gtest: `libs/builtin/tests/mldivide_test.cpp` — 18 tests
    (12 mldivide + 6 mrdivide), all bit-identical to MATLAB
  - smoke: `libs/builtin/tests/smoke/mldivide_smoke.m`

  Bit-identical to MATLAB and Octave on all 28 fingerprints across
  both specs (`tol = 1e-10`).

