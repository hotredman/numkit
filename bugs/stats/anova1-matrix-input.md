# stats.anova1 — matrix input form unsupported

- **Status:** ✅ FIXED (2026-06-03, lib-dev cycle c179)
- **Severity:** P2 (missing input form)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE
- **Fix:** `anova1_reg` detects a matrix first arg (`rows>1 && cols>1`),
  stacks the columns into `(y, group)` with `group` = 1-based column index,
  then runs the existing one-way ANOVA (NaNs dropped by `bucket()`). Verified
  p, SS, F vs MATLAB. Guard: `libs/stats/tests/anova1_test.cpp`.

## Symptom
MATLAB `anova1(X)` accepts a **matrix** whose columns are the groups.
numkit requires the `(y, group)` form and throws on a bare matrix.

## Repro
```matlab
anova1([1 2 3; 2 3 4; 3 4 5])
% numkit: Error — anova1: requires (y, group[, 'off'])
% MATLAB: p = 0.125000   (3 columns treated as 3 groups of 3)
```

## Root cause
`anova1` adapter only handles the vector `y` + grouping `group` signature;
the column-per-group matrix overload isn't wired.

## Suggested fix
When the first arg is a matrix (≥2 columns), treat each column as a group
(stack into y + a synthesized column-index group vector), then run the
existing one-way ANOVA. Cheap input-form add. Verify p / table against
MATLAB for unequal-but-rectangular and the matrix-with-NaN cases.

## References
- `libs/stats/src/.../anova1*`
- MATLAB `doc anova1`
