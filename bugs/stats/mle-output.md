# stats.mle — missing 2nd output (parameter confidence intervals `pci`)

- **Status:** 🔴 OPEN
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

## References
- `libs/stats/src/.../mle*`
- MATLAB `doc mle`
- related: `normfit`/`expfit`/`poissfit` already return CIs
