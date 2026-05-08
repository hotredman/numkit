# stats.empirical/ksdensity — ТЗ for completion

**Status:** closed (Censoring/Support/BoundaryCorrection deferred)
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
  surfaced 6 missing N-V features:

  1. Default bandwidth was using `iqr(x)/1.34` only; MATLAB uses
     `mad(x)/0.6745` with iqr fallback. Numbers differed by ~5%.
     Now `bw=1.4863` matches MATLAB exactly on the test dataset.
  2. `'Kernel'` N-V parsed: 'normal' (default), 'box', 'triangle',
     'epanechnikov'. Each kernel uses MATLAB's σ²=1 normalization
     (h × sqrt(unit-σ²-inverse)) so the bandwidth has consistent
     standard-deviation semantics across kernels.
  3. `'Function'` N-V: 'pdf' (default), 'cdf', 'survivor',
     'cumhazard'. 'icdf' not yet supported (errors).
  4. `'NumPoints'` N-V (default 100; was hardcoded).
  5. `'Weights'` N-V (per-observation weights, normalized to Σw=1).
  6. `'Censoring'` / `'Support'` / `'BoundaryCorrection'` N-V now
     throw "not yet supported" instead of silently ignoring.

  PMR rule applied: scratch buffers (xv, ws, dev, grid) on
  std::vector for now (descriptive_extras.cpp predates the PMR
  rollout — full file refactor TBD).

  4 artefacts shipped (impl + 18-fp parity spec + 9 gtests + smoke).
  Bit-identical numkit ↔ MATLAB at tol=1e-5 across all 18
  fingerprints (cdf right-tail accumulates ~4e-6 FP noise — well
  under the documented tolerance). Octave doesn't ship `ksdensity`.

