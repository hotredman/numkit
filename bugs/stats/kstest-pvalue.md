# stats.kstest — p-value and critical value wrong (statistic is correct)

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P1 (wrong result)
- **Kind:** bug
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

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 23), `toolboxes/stats/src/test/hypothesis.cpp`.
- **kstest two-sided p** — exact `1 − K(n, D)` via the **Marsaglia-Tsang-Wang
  (2003)** matrix method, with MATLAB's corrected asymptotic
  `2·exp(−(2.000071 + .331/√n + 1.409/n)·n·D²)` when `s = n·D² > 7.24` or
  `(s > 3.76 && n > 99)`.
- **kstest one-sided p** (`'larger'`/`'smaller'`) — exact **Birnbaum-Tingey
  (1951)** survival on the directional statistic.
- **Critical value** — by inverting the matching p-function (bisection); this
  reproduces MATLAB's exact kstest critical-value table to its 5-significant-
  figure precision (n=6: 0.51926/0.46799/0.61661 at α=0.05/0.10/0.01). MATLAB
  stores a rounded table, so cv is validated at 1e-4 (gtest) rather than 1e-6.
- **kstest2** — **Stephens' corrected asymptotic**
  `λ = (√ne + 0.12 + 0.11/√ne)·D`: two-sided `2·Σ(−1)^{k−1}exp(−2λ²k²)`,
  one-sided `exp(−2λ²)`.
- Verified vs MATLAB R2025b (~1e-9 on the p-values): n=6 two-sided
  p=0.94998410 cv=0.51926; one-sided 0.97197377/0.57170523; n=12 p=0.98282723;
  kstest2 two-sided 0.84705434, one-sided 0.47200535.
- Live guard: `toolboxes/stats/tests/kstest_exact_test.cpp` (6 TEST_F) + flipped
  `StatsKnownBug.KstestPValue` / `Kstest2PValue` live. Parity:
  `tools/parity/specs/kstest_exact.json` (correctness=OK). Smoke:
  `toolboxes/stats/tests/smoke/kstest_exact_smoke.m`.
- Still OPEN (separate, minor, noted above): jbtest/adtest tail-p clamping to
  MATLAB's documented table bounds — cosmetic, not tracked here.

## References
- `toolboxes/stats/src/test/hypothesis.cpp` (kstest, kstest2, marsagliaK, birnbaumTingey)
- MATLAB `toolbox/stats/stats/kstest.m`
