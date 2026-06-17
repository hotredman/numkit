# stats.isoutlier — 'gesd' method throws

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (missing option)
- **Kind:** stub
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`isoutlier(x, 'gesd')` (generalized extreme Studentized deviate test)
throws. (median/mean/quartiles/grubbs/movmedian/movmean are implemented.)

## Repro
```matlab
isoutlier([1 2 3 4 5 6 7 8 9 50], 'gesd')
% numkit: Error — isoutlier: method 'gesd' is not supported in this revision
% MATLAB: 0 0 0 0 0 0 0 0 0 1
```

## Root cause
`src/toolboxes/stats/src/descriptive/descriptive_extras.cpp` `isoutlier_reg` throws
for `'gesd'` (only that method remains stubbed after movmedian/movmean were
added).

## Suggested fix
Implement GESD: iteratively compute the max studentized deviate
`R_i = max|x-mean|/std`, compare to the critical value
`λ_i = (n-i)·t / sqrt((n-i-1+t²)(n-i+1))` with
`t = tinv(1 - α/(2(n-i+1)), n-i-1)`, for `i = 1…MaxNumOutliers` (default
`MaxNumOutliers` ≈ `floor(n/2)`?? — verify), then flag the largest run for
which `R_i > λ_i`. `ThresholdFactor` is the significance `α` (default 0.05).
Reuse the `grubbs` scaffolding already present. Moderate; verify the
`MaxNumOutliers` default + the exact critical-value form against MATLAB.

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 24),
  `src/toolboxes/stats/src/descriptive/descriptive_extras.cpp` (`gesdColumnMask`).
- Rosner's GESD: peel the most-extreme studentized deviate
  `R_i = max|x−mean|/std` for `i = 1…MaxNumOutliers`, comparing each to the
  critical value `λ_i` which equals the **Grubbs** form at the current sample
  size `m = N−i+1`: `λ_i = ((m−1)/√m)·√(t²/((m−2)+t²))`,
  `t = tinv(α/(2m), m−2)` — so the existing `grubbsColumnMask` formula is
  reused. Unlike Grubbs it does NOT stop at the first non-exceedance: the
  number of outliers is the **largest** `i` with `R_i > λ_i`, which un-masks
  multiple mutually-inflating outliers.
- **`MaxNumOutliers` default** verified vs MATLAB = `max(1, round(n/10))`
  (n=4→1, n=5→1, n=20→2, n=25→3, n=30→3, n=100→10). Capped at `N−2`.
  **`ThresholdFactor`** = significance level α (default 0.05). Both options
  parse in `isoutlier_reg`; `MaxNumOutliers` is rejected for other methods.
- Verified vs MATLAB R2025b (exact masks): repro → only the 50; mid-vector
  `[1 2 3 100 4 5]` → the 100; clean → none; spread `[0…20 30 40]` (n=25) peels
  3; masking `[0…100 101 102 103 104]` (n=15) → 0 by default but
  `MaxNumOutliers=5` → 5; `ThresholdFactor=0.01`; small n=5.
- Live guard: `src/toolboxes/stats/tests/isoutlier_gesd_test.cpp` (7 TEST_F) + flipped
  `StatsKnownBug.IsoutlierGesd` live; stale throw-test in
  `missing_data_test.cpp` rewritten. Parity:
  `tools/parity/specs/isoutlier_gesd.json` (correctness=OK). Smoke:
  `src/toolboxes/stats/tests/smoke/isoutlier_gesd_smoke.m`.
- Note: `rmoutliers(x,'gesd')` and `filloutliers(...,'gesd')` remain script-only
  / deferred (separate adapters; out of scope here).

## References
- `src/toolboxes/stats/src/descriptive/descriptive_extras.cpp` (isoutlier_reg, grubbs, gesdColumnMask)
- MATLAB `doc isoutlier` ('gesd')
