# stats.kstest — p-value and critical value wrong (statistic is correct)

- **Status:** 🔴 OPEN
- **Severity:** P1 (wrong result)
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`kstest` returns the correct KS **statistic** but the wrong **p-value** and
**critical value** — numkit uses a plain asymptotic Kolmogorov formula,
while MATLAB uses the EXACT small-sample distribution.

## Repro
```matlab
x = [-1 0 1 2 -0.5 0.5];
[h, p, ksstat, cv] = kstest(x);
% ksstat: numkit 0.19146246 == MATLAB 0.19146246   (statistic OK)
% p:      numkit 0.98041424  vs  MATLAB 0.9499841   (WRONG)
% cv:     numkit 0.55444145  vs  MATLAB 0.51926     (WRONG)
% n=50 sample: p numkit 0.99566 vs MATLAB 0.99190   (closer, still off)
```

## Root cause
The statistic D = max|F̂ - F| matches. The D→p / α→cv mapping differs:
numkit uses an asymptotic series; MATLAB (`toolbox/stats/stats/kstest.m`)
uses the EXACT finite-n Kolmogorov distribution for small n (Marsaglia /
Miller — lines ~130-157, and a Miller critical-value table via spline,
lines ~175-214), falling back to the corrected asymptotic
`2*exp(-(2.000071+.331/sqrt(n)+1.409/n)·nD²)` only for large n.

## Suggested fix
NOT simple. Implement the exact small-n KS distribution (Marsaglia-Tsang-Wang
matrix method) for the p-value, and the Miller exact critical-value table
(spline-interpolated in α) for `cv`; use the corrected asymptotic for large
n. Validate p and cv vs MATLAB across n = 5…200.

**`kstest2` has the same defect** (found 2026-06): the 2-sample KS
statistic is correct but the p-value diverges —
`kstest2([1 2 3 4 5],[2 3 4 5 6 7])` gives ks=0.3333 (correct) but p=0.9223
vs MATLAB 0.8471. Same fix family (asymptotic vs exact/corrected KS
distribution). Related (separate, minor): jbtest / adtest match MATLAB
inside the usable range but their tail p-values are not clamped to MATLAB's
documented table bounds ([0.001, 0.5] for jbtest) — cosmetic, lower priority.

## References
- `libs/stats/src/.../kstest*`
- MATLAB `toolbox/stats/stats/kstest.m`
