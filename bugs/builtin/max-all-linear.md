# builtin.max / min — 'all' + 'linear' combined option errors

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing option combo)
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`[m, i] = max(A, [], 'all', 'linear')` (max over all elements, returning the
linear index) throws. `max(A,[],'all')` and `max(A,[],dim,'linear')` each
work alone — only the combination fails.

## Repro
```matlab
[m, i] = max([3 1; 4 1; 2 9], [], 'all', 'linear')
% numkit: Error — Cannot convert char to scalar
% MATLAB: m = 9, i = 6   (linear index of the 9 at (3,2))
```

## Root cause
The max/min argument parser (`libs/builtin/src/math/arithmetic/
reductions.cpp`) handles a single trailing option string but mis-parses the
second one: after consuming `'all'` it treats `'linear'` as the `dim`
argument and calls `toScalar` on the char vector → "Cannot convert char to
scalar".

## Suggested fix
In the max/min option parser, accept BOTH `'all'` and `'linear'` trailing
strings (in either order), and when both are present return the linear index
into the flattened array (the index `i` such that `A(i) == m`). Moderate —
the reducer's all-reduction already finds the max; it needs to also report
the flat index. Validate `m` and `i` vs MATLAB for 2-D and 3-D inputs.

## References
- `libs/builtin/src/math/arithmetic/reductions.cpp` (max/min arg parsing)
- MATLAB `doc max` ('all', 'linear')
