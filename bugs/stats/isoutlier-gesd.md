# stats.isoutlier — 'gesd' method throws

- **Status:** 🔴 OPEN
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
`libs/stats/src/descriptive/descriptive_extras.cpp` `isoutlier_reg` throws
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

## References
- `libs/stats/src/descriptive/descriptive_extras.cpp` (isoutlier_reg, grubbs)
- MATLAB `doc isoutlier` ('gesd')
