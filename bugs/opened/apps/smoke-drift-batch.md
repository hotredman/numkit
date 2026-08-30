# apps.smokes — 11 smokes red on main (two residual classes after the complex-linalg split)

- **Status:** 🔴 OPEN (triage list)
- **Severity:** P2 (test-suite drift + one fixture-path class)
- **Kind:** bug
- **Found:** 2026-08-30 via the full 710-smoke sweep (699 pass; the complex-linalg cluster is filed separately as `opened/linalg/complex-linalg-regression.md`)

## Symptom

11 of 710 smokes fail on main, identically with and without `--compat` —
nothing to do with the import-compat cleanup; found by it.

## Triage

| class | smokes | evidence |
|---|---|---|
| complex linalg (filed separately) | chol_complex, schur_complex, funm_parlett, decomposition, matrix_functions_general | dtype-rejection on complex operands → `opened/linalg/complex-linalg-regression.md` |
| fixture path missing `src/` prefix | imread_tiff, region | `imread('toolboxes/image/tests/fixtures\gray8.tif')` — the fixture EXISTS at `src/toolboxes/image/tests/fixtures/`; the smoke's relative path only resolves with CWD=`src/`, but the documented run command executes from the repo root |
| API-shape drift (ttest family) | hypotest, ttest_extras, vartest_extras, cummax_cummin | `Cannot convert struct to scalar (in call to 'fprintf')` — the smoke prints a component of what is now a struct return; either the return shape changed (engine) or the smokes predate it (test drift) |

## Repro

```matlab
clear;
% (representative, ttest class — engine level)
h = ttest([5 6 7 8]);
fprintf('%g\n', h.pval)
% numkit (current): depends on the ttest return shape — the smokes assume scalar components
% MATLAB: prints the p-value
```

Fixture class repro: run
`numkit_repl.exe src/toolboxes/image/tests/smoke/imread_tiff_smoke.m` from
the repo root (documented convention) → file-not-found on a fixture that
exists under `src/`.

## Suggested fix

Decide per class: fixture smokes either get repo-root-relative paths or the
documented run command gains a CWD note; the ttest-family smokes need the
actual current return shape checked against MATLAB (if numkit's shape
diverges from MATLAB, that is an engine bug, not smoke drift — split it out
then). Add the smoke corpus to a CI/scheduled sweep so drift is caught same-day.

## References
- **Guard:** deferred — umbrella triage; guards land per diagnosed class.

Sweep logs: this session (710 files, with/without `--compat`); related
consolidation commits `41c0f328`, `e95e6054`.
