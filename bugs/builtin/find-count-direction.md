# builtin.find — count `k` and 'first'/'last' direction ignored

- **Status:** 🔴 OPEN
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
`find_reg` (`libs/builtin/src/language/arrays/matrix.cpp:3405`): the
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
- `libs/builtin/src/language/arrays/matrix.cpp` (find_reg, ~line 3405)
- MATLAB `doc find` (k, direction)
