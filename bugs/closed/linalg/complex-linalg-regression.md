# linalg.complex — chol (and the complex linalg family) rejects complex input again — regression of the closed complex-matrix support

- **Status:** ✅ FIXED (chol root; see below — the smoke cluster dissolved into separate issues)
- **Severity:** P1 wrong result (errors where MATLAB returns a value)
- **Kind:** bug
- **Found:** 2026-08-30 via the full smoke sweep during the import-compat cleanup (6 linalg smokes red; nobody had run the smoke corpus since the builtin consolidation)

## Symptom

`chol` rejects a Hermitian complex matrix with `Not a double array`. The
complex-matrix support this regresses was CLOSED 2026-08-05
(`closed/linalg/complex-matrix-unsupported.md` — "full complex matrix support
across linalg (eig, svd, qr, lu, chol, det, inv, rank, pinv, norm)").

## Repro

```matlab
clear;
A = [2, 1i; -1i, 2];
R = chol(A);
disp(R(1,1))
% numkit: Error: Not a double array (in call to 'chol')
% MATLAB: 1.4142 (complex Cholesky works)
```

## Resolution (2026-08-30)

The reg-layer `chol_reg` reimplemented the factor with `A.doubleData()`
(double-only) instead of instantiating the already complex-aware kernels
(`cholUpperFactor<T>` / `transposeSquare<T>` in decompositions_detail.hpp).
Fixed by making the reg type-generic (real + complex instantiation); the
guard `CholAcceptsComplexHermitian` is live and green; `chol_complex_smoke`
passes; full Release suite green.

The remaining four red linalg smokes are NOT this root — each is its own
issue, tracked under `opened/apps/smoke-drift-batch.md`:
- `schur_complex` — no complex Schur KERNEL exists (complex QR iteration unimplemented; eig has complex paths, schur does not);
- `funm_parlett` — "Array indices must be positive integers" (indexing, not dtype);
- `decomposition` — the `decomposition()` builtin is unregistered (missing-fn regression);
- `matrix_functions_general` — `eig` claims "only symmetric / Phase 2b deferred" though the real non-symmetric Schur (Francis QR) was implemented — a wiring gap.

## Affected smoke set (at filing — suspected shared root)

`chol_complex_smoke.m`, `schur_complex_smoke.m`, `funm_parlett_smoke.m`,
`decomposition_smoke.m`, `matrix_functions_general_smoke.m` — all fail with
dtype-rejection errors on complex operands.

## Root cause (suspected)

The builtin consolidation (`41c0f328` / `e95e6054` — the same family that
caused the bsxfun single-shot regression) likely re-registered the linalg
entry points with double-only argument validation, dropping the complex
overload routing. Verify against the pre-consolidation registration.

## Suggested fix

Restore complex dispatch in the affected entry points; re-open coverage by
running the six smokes. The closed bug's tests should be re-checked — the
gtest suite passed while the smokes fail, so a gtest coverage gap likely
exists too (chol complex path may be gtest-untested).

## References
- **Guard:** `DISABLED_CholAcceptsComplexHermitian`

`closed/linalg/complex-matrix-unsupported.md` (the original fix);
smoke sweep evidence: this file; DISABLED_ regression test:
`src/toolboxes/linalg/tests/known_bugs_test.cpp`.
