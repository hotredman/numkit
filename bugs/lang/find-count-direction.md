# lang.find — count `k` and 'first'/'last' direction ignored

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P1 (wrong result — silently returns too many indices)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (option sweep)

## Symptom
`find(X, k)` should return the first `k` indices of nonzero elements;
`find(X, k, 'last')` the last `k`. numkit ignores both the count and the
direction and always returns ALL nonzero indices. (The single-element-result
idiom `find(x, 1)` / `find(x, 1, 'last')` is extremely common.)

## Repro
```matlab
find([0 1 0 1 1], 2)
% numkit: [2 4 5]
% MATLAB: [2 4]
find([0 1 0 1 1], 1, 'last')
% numkit: [2 4 5]
% MATLAB: 5
find([0 1 0 1 1], 2, 'last')
% numkit: [2 4 5]
% MATLAB: [4 5]
```
`find(X)` (no count) and `[r,c]=find(X)` subscripts are correct.

## Root cause
`find_reg` (`src/lang/src/arrays/matrix.cpp:3405`): the
single-output path calls `find(x, mr)` and never inspects `args[1]` (count)
or `args[2]` (`'first'`/`'last'`). The multi-output branch even carries a
comment that "the find(X, n) row-count limit is not applied here".

## Suggested fix
Parse `args[1]` as a positive integer count `k` and `args[2]` as the
direction (`'first'` default / `'last'`). After collecting the nonzero linear
indices, keep the first `k` ('first') or the last `k` ('last'); `k`
exceeding the count returns all. Apply to BOTH the single-output and the
`[r,c]`/`[r,c,v]` multi-output forms. Small and mechanical.

## References
- `src/lang/src/arrays/matrix.cpp` (find_reg, ~line 3415)
- MATLAB `doc find` (k, direction)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 1).
- `find_reg` now parses `args[1]` as a positive-integer count `K` and
  `args[2]` as the direction (`'first'` default / `'last'`), collects the
  nonzero linear indices, and keeps the first/last `K` — applied to BOTH the
  single-output (linear index) and `[r,c]` / `[r,c,v]` forms. `K` exceeding
  the count returns all; `'last'` returns the last `K` in ascending order;
  `K` must be a positive scalar integer (matches MATLAB `find(X,0)` error).
  Single-output path also switched to the type-complete `forEachNonzero`
  (so complex single-output `find` no longer reads real storage).
- Live regression guard: `tests/builtin/find_count_direction_test.cpp`
  (10 cases). Parity: `tools/parity/specs/find.json` (correctness=OK).
  Smoke: `tests/builtin/smoke/find_count_direction_smoke.m`.
