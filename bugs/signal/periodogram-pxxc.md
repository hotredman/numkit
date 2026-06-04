# signal.periodogram — confidence-interval output (pxxc) unsupported

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing output / option)
- **Kind:** missing-output
- **Found:** 2026-06-04 via DEEP-PROBE (multi-output sweep)

## Symptom
`[pxx, f, pxxc] = periodogram(..., 'ConfidenceLevel', p)` throws. MATLAB
returns `pxxc`, the two-column confidence interval for the PSD estimate.
(pwelch has the same gap — see deferred note.)

## Repro
```matlab
[pxx, f, pxxc] = periodogram([1 2 3 4 5 6 7 8], [], [], 1, 'ConfidenceLevel', 0.95)
% numkit: Error — Cannot convert double to scalar
% MATLAB: pxxc is an (nf × 2) matrix of [lower, upper] PSD bounds
```

## Root cause
The `'ConfidenceLevel'` name-value pair is not parsed (the adapter treats the
value as a positional numeric → "Cannot convert double to scalar"), and the
`pxxc` 3rd output is not computed.

## Suggested fix
Parse `'ConfidenceLevel'`; compute the chi-square-based CI for the
periodogram PSD (MATLAB uses the equivalent degrees of freedom from the
window). Thread `pxxc` as the 3rd output. Same machinery would close the
pwelch/cpsd CI gap (deferred-gap R). Moderate.

## References
- `libs/signal/src/.../periodogram*`
- MATLAB `doc periodogram` (ConfidenceLevel / pxxc)
