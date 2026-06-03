# stats.dwtest — p-value differs from MATLAB (statistic is correct)

- **Status:** 🔴 OPEN
- **Severity:** P2 (wrong p-value)
- **Found:** 2026-06 via DEEP-PROBE (stats coverage)

## Symptom
`dwtest` returns the correct Durbin–Watson statistic but a p-value that
diverges from MATLAB — numkit uses an approximation where MATLAB uses the
exact distribution.

## Repro
```matlab
[p, dw] = dwtest([1 2 1 3 2 4]', [ones(6,1) (1:6)']);
% dw: numkit 0.3142857 == MATLAB 0.3142857   (statistic OK)
% p:  numkit 0.01724309  vs  MATLAB 0          (≈0; differs by orders of magnitude)
```

## Root cause
The DW statistic matches; the p-value method differs. MATLAB's `dwtest`
default computes the EXACT p-value via Pan's algorithm (the distribution of
a ratio of quadratic forms in normal variables) for small/medium n; numkit
uses a normal/beta approximation.

## Suggested fix
Implement the exact DW p-value (Pan's algorithm / Imhof's method — numerical
inversion of the characteristic function of the quadratic-form ratio), with
the asymptotic approximation as a fallback for large n. Moderate. Validate p
vs MATLAB across n and both tails ('left'/'right'/'both').

## References
- `libs/stats/src/.../dwtest*`
- MATLAB `doc dwtest`
