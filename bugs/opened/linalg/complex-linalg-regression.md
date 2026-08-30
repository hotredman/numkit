# linalg.complex — chol (and the complex linalg family) rejects complex input again — regression of the closed complex-matrix support

- **Status:** 🔴 OPEN
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

## Affected smoke set (same suspected root — complex routing lost in the builtin consolidation)

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
