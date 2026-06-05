# stats.dwtest — p-value differs from MATLAB (statistic is correct)

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (wrong p-value)
- **Kind:** bug
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

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 22), `libs/stats/src/test/ad_dw.cpp`.
- **Exact p-value via Imhof (1961).** Under H0 the residuals are `e = Mε`,
  `M = I − X(X'X)⁻¹X'`; `DW < d ⟺ ε'M(A−d·I)Mε < 0` where `A` is the
  first-difference tridiagonal. The `n−k` residual-space eigenvalues `λ_j` of
  `MAM` (via `numkit::linalg::eig_symmetric`, dropping the `k` structural zeros)
  give `Q = Σ(λ_j − dw)Z_j²`, and `pLeft = P(Q<0) = ½ − (1/π)∫₀^∞ g(u)du` is
  evaluated by mapping the tail to `[0,1)` (`u = t/(1−t)`) and composite Simpson.
  Matches MATLAB's default `'exact'` method to **~1e-9**.
- **'Tail' option:** `'right'` (positive autocorrelation) = `pLeft`; `'left'`
  (negative) = `1−pLeft`; `'both'` (default) = `2·min(pLeft, 1−pLeft)`.
- Verified vs MATLAB R2025b: low-DW `[1 2 1 3 2 4]'` both≈0 right≈0 left=1;
  high-DW `[1 -1 …]'` (n=8) dw=3.5 both=0.005693520875 right=0.9971532396
  left=0.002846760438; second design both≈0.
- `'Method','approximate'` is accepted but is numkit's own beta moment-fit (NOT
  MATLAB-identical — MATLAB's 'approximate' is a different algorithm); the
  default `'exact'` is the MATLAB-matching path. Documented, not validated.
- Live guard: `libs/stats/tests/dwtest_exact_test.cpp` (5 TEST_F) + flipped
  `StatsKnownBug.DwtestPValue` live. Parity:
  `tools/parity/specs/dwtest_exact.json` (correctness=OK) +
  `adtest_dwtest.json` comment updated. Smoke:
  `libs/stats/tests/smoke/dwtest_exact_smoke.m`.

## References
- `libs/stats/src/test/ad_dw.cpp` (dwtest, dwExactPLeft, imhofPLeft)
- MATLAB `doc dwtest`
