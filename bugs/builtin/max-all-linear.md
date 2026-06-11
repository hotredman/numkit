# builtin.max / min — the 'all' option was entirely broken

- **Status:** ✅ FIXED (lib-dev, 2026-06)
- **Severity:** P1 (very common idiom threw)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`max(A, [], 'all')` and `min(A, [], 'all')` — reduce over EVERY element, a
very common idiom — threw "Cannot convert char to scalar". (Originally
mis-filed as only the `'all','linear'` combo; in fact plain `'all'` failed
too, on 2-D and N-D alike. `sum(A,'all')` worked, but max/min did not.)

## Repro (pre-fix)
```matlab
max([1 2; 3 4], [], 'all')                 % numkit: Error "Cannot convert char to scalar"
[m, i] = max([3 1; 4 1; 2 9], [], 'all')   % MATLAB: m = 9, i = 6 (linear index)
max(reshape(1:24,2,3,4), [], 'all')        % MATLAB: 24
```

## Root cause
`max_reg`/`min_reg` (`toolboxes/builtin/src/math/arithmetic/reductions.cpp`)
parsed the reduction dim with `args[2].toScalar()`, which throws on the
`'all'` char vector. There was no 'all' (reduce-all-elements) path.

## Fix
Detect the `'all'` string in `max_reg`/`min_reg`; reduce over the flattened
array (`reshape(A, numel, 1)` then the existing min/max reduce). The column
position IS the linear index — matching MATLAB's `'all'` 2nd output (always
linear), so `'all'` and `'all','linear'` give the same result. Works for
2-D, N-D, and `'omitnan'`. 4 artefacts: impl + parity correctness=OK
(max_min_all.json) + gtest (MathReductionsBatchTest.MaxMinAll) + smoke.

## References
- `toolboxes/builtin/src/math/arithmetic/reductions.cpp` (max_reg, min_reg)
- `tools/parity/specs/max_min_all.json`
- `toolboxes/builtin/tests/math_reductions_batch_test.cpp`
- MATLAB `doc max` ('all', 'linear')
