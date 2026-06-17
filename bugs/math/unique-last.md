# math.unique — 'last' option ignored (returns first-occurrence indices)

- **Status:** ✅ FIXED (2026-06-03, lib-dev cycle c180) — sorted order; one
  rare sub-gap deferred (see Remaining).
- **Severity:** P1 (wrong result — option ignored)
- **Kind:** bug
- **Severity note:** affects `ia` only; `C` and `ic` are correct.
- **Found:** 2026-06 via DEEP-PROBE
- **Fix:** threaded a `bool last` flag from `unique_reg` into
  `uniqueWithIndices` / `uniqueComplexFull` / `uniqueRowsWithIndices`; the
  sorted paths record the LAST occurrence (`map[key]=i`) instead of
  `try_emplace`. Verified `ia`/`ic` vs MATLAB for vector, complex, `'rows'`
  and NaN inputs. Default ('first') byte-identical. Guard:
  `tests/builtin/unique_last_test.cpp`.

## Remaining (deferred sub-gap)
`unique(A,'stable','last')`: MATLAB R2025b does NOT error (the original note
below was wrong) — it orders the unique values by their LAST occurrence, e.g.
`unique([3 1 2 1 3],'stable','last')` → `C=[2 1 3]`, `ia=[3 4 5]`. numkit
currently returns the `'stable'` (first-occurrence) order here. Rare combo;
tracked by `DISABLED_UniqueStableLast` in `tests/builtin/known_bugs_test.cpp`.

## Symptom
`[C, ia] = unique(A, 'last')` returns the index of the FIRST occurrence of
each value in `ia` instead of the LAST — the `'last'` flag is parsed but
ignored.

## Repro
```matlab
[C, ia] = unique([3 1 2 1 3], 'last')
% numkit: C = [1 2 3], ia = [2 3 1]   (first occurrence — WRONG)
% MATLAB: C = [1 2 3], ia = [4 3 5]   (last occurrence of 1@4, 2@3, 3@5)
```
The default ('first') is correct, and `'stable'` works.

## Root cause
`src/math/src/discrete/discrete.cpp` records first occurrence via
`firstIdx.try_emplace(p[i], i)` (keeps the first) in every path
(`uniqueWithIndices` sorted, `uniqueComplexFull`, `uniqueRows`); the
`'last'` token isn't threaded from `unique_reg`.

## Suggested fix
Thread a `bool last` flag from `unique_reg` into the three unique paths;
when set, record the LAST occurrence (`firstIdx[p[i]] = i;` instead of
`try_emplace`). `'last'` applies to the SORTED mode only — MATLAB errors on
`'stable','last'` together, so reject that combo. Default ('first') stays
byte-identical. Mechanical but spans 3 paths (sorted / complex / rows) so
not a one-spot change. Validate `ia` for vector, complex, and `'rows'`.

## References
- `src/math/src/discrete/discrete.cpp` (uniqueWithIndices,
  uniqueComplexFull, uniqueRows, unique_reg)
- MATLAB `doc unique` ('last')
