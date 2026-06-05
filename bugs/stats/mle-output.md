# stats.mle — missing 2nd output (parameter confidence intervals `pci`)

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (missing output)
- **Kind:** missing-output
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`[phat, pci] = mle(data, ...)` throws "Too many output arguments". numkit
returns only the parameter estimates `phat`; the confidence-interval matrix
`pci` is missing.

## Repro
```matlab
[phat, pci] = mle([2 3 4 5 6 4 3])
% numkit: Error — Too many output arguments
% MATLAB: pci(1,1) = 2.613054  (95% CI lower bound of the mean estimate)
```

## Root cause
The `mle` adapter emits only `outs[0]`. `pci` (default 95% CIs) is not
computed/returned.

## Suggested fix
Compute `pci` from the estimator's asymptotic covariance (the inverse Fisher
information / Hessian at the MLE) per distribution, default α = 0.05.
NOTE: `mle` takes a FnHandle for custom pdfs, so the public-API shape may
keep it script-only — at minimum make `mle_reg` nargout-aware and emit pci
for the built-in distributions. Verify per-distribution CIs vs MATLAB.

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 27), `libs/stats/src/fit/fit.cpp`
  (`mle_reg`).
- `mle_reg` is now nargout-aware and parses an `'Alpha'` option (default 0.05).
  For `nargout >= 2` it emits `pci` — a 2×k matrix (row 1 = lower bounds, row 2
  = upper bounds, one column per parameter) — by **reusing the matching `*fit`
  CI machinery**, which already matches MATLAB exactly (the md's hint):
  normal → `normfit` (mu CI / sigma CI); exponential → `expfit`; poisson →
  `poissfit`; lognormal → `normfit(log(data))`.
- Verified vs MATLAB R2025b: normal pci `[2.613054 0.866829; 5.101232
  2.962187]`, exponential `[0.779889; 4.132805]`, poisson `[1.745217;
  4.412625]`, lognormal `[0.162710 0.362113; 1.202134 1.237439]`, and the
  `'Alpha', 0.01` widening (`pci(1,1)=1.972167`). `phat` and the 1-output form
  are unchanged.
- Custom `'pdf'/'logpdf'/'nloglf'` fitting (and its CI) stays deferred.
- Live guard: `libs/stats/tests/mle_pci_test.cpp` (6 TEST_F) + flipped
  `StatsKnownBug.MleConfidenceIntervals` live. Parity:
  `tools/parity/specs/mle.json` extended (correctness=OK). Smoke:
  `libs/stats/tests/smoke/mle_pci_smoke.m`.

## References
- `libs/stats/src/fit/fit.cpp` (mle_reg)
- MATLAB `doc mle`
- related: `normfit`/`expfit`/`poissfit` already return CIs
