# stats.empirical/ecdf — ТЗ for completion

**Status:** closed (Censoring/Kaplan-Meier deferred)
**Priority:** low
**Effort:** small
**Audited at commit:** f92087f
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Numbers match MATLAB exactly on probed
inputs.

## Recommended fixes

1. **Spec extension** — fingerprint over more inputs and N-V
   options. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-08
- Notes: Auditor's "no major gap" claim was wrong — re-probe
  surfaced 5 documented N-V features that were entirely missing:

  1. `'Function'` mode N-V parsed: `'cdf'` (default), `'survivor'`
     (= 1 - cdf), `'cumulative hazard'` (Nelson-Aalen estimator
     `H(x) = Σ d_i/n_i`).
  2. `'Frequency'` weighting via per-observation counts; `N_eff =
     Σ freq`, cumulative weighted partition.
  3. `'Alpha'` significance level for confidence bounds (default 0.05).
  4. 4-output form `[f, x, flo, fup]` returns Greenwood-style
     binomial Wald CI with z = `norminv(1 - α/2)`. First/last rows
     emit NaN bounds (matches MATLAB).
  5. `'Censoring'` arg detected and throws "not yet supported"
     instead of silently producing wrong values (Kaplan-Meier
     estimator deferred).

  Other N-V (IterationLimit / Tolerance / ICMFrequency / Bounds)
  silently accepted as no-ops since they only matter for the
  Kaplan-Meier path.

  4 artefacts shipped (impl + 19-fp parity spec + 6 gtests + smoke
  via existing ecdf smoke). Bit-identical numkit ↔ MATLAB ↔
  Octave on all 19 fingerprints across all 5 modes.

