# lang.sort — 'MissingPlacement' option silently ignored

- **Status:** ✅ FIXED (lib-dev, 2026-06)
- **Severity:** P1 (wrong result — option ignored)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`sort(X, ..., 'MissingPlacement', 'first'|'last')` was parsed but ignored —
numkit always used the direction-dependent default ('auto'). So ascending +
`'first'` left NaN at the END (should be FIRST), and descending + `'last'`
left it at the front.

## Repro (pre-fix)
```matlab
sort([3 NaN 1 2], 'MissingPlacement', 'first')
% numkit (pre-fix): 1 2 3 NaN     (NaN stayed last — WRONG)
% MATLAB:           NaN 1 2 3
sort([3 NaN 1 2], 'descend', 'MissingPlacement', 'last')
% numkit (pre-fix): NaN 3 2 1     MATLAB: 3 2 1 NaN
```
The 'auto' default (NaN last for ascending, first for descending) was
already correct and matches MATLAB.

## Root cause
`sort_reg` (`src/lang/src/arrays/matrix.cpp`) skipped the
`'MissingPlacement'` name-value token, and `sort`/`sortComplex` hard-coded
the NaN side as `descend ? an : bn` (auto only).

## Fix
Added `enum class NanPlace { Auto, First, Last }` + a 5-arg `sort` overload
(4-arg delegates with `Auto`, so existing behaviour is byte-identical).
Both comparators (real + complex) now use a `nanFirst` flag:
`Auto -> descend`, `First -> true`, `Last -> false`. `sort_reg` parses the
`'MissingPlacement'` NV pair. 4 artefacts: impl + parity (sort_missingplacement.json,
correctness=OK) + gtest (SortFindTest.SortMissingPlacement, TW+VM) + smoke.

## References
- `src/lang/src/arrays/matrix.cpp` (sort, sortComplex, sort_reg)
- `src/lang/include/numkit/lang/arrays/matrix.hpp`
- MATLAB `doc sort` ('MissingPlacement')
