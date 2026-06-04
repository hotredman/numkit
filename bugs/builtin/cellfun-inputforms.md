# builtin.cellfun — multiple cell arrays + string function-name forms unsupported

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing input forms)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (function-handle sweep)

## Symptom
Two MATLAB `cellfun` input forms are rejected:

1. **Multiple cell arrays** — `cellfun(fn, C1, C2, ...)` applies
   `fn(C1{i}, C2{i}, ...)`. numkit treats the second cell as an option name.
   (`arrayfun` already supports the multi-array form, so this is an
   asymmetry.)
2. **String function name** (legacy) — `cellfun('isempty', C)` /
   `cellfun('length', C)` etc. numkit requires a function handle.

## Repro
```matlab
cellfun(@(a,b) a+b, {1,2}, {10,20})
% numkit: Error — cellfun: option name without value
% MATLAB: [11 22]

cellfun('isempty', {[],[1],[]})
% numkit: Error — cellfun: fn argument must be a function handle
% MATLAB: [1 0 1]

cellfun('length', {[1 2],[1 2 3]})
% MATLAB: [2 3]
```

## Root cause
`cellfun_reg` (`libs/builtin/src/language/cells/cell.cpp`) accepts exactly
one cell array followed by name/value options, and `cellfun()` takes a single
`FnHandle` + one cell. There is no path to (a) collect several leading cell
arrays and zip them into the callback, nor (b) accept a string builtin-name
as the first argument.

## Suggested fix
- Multi-cell: collect all leading cell-array positional args (until the first
  string option), require equal sizes, and pass `{C1{i}, C2{i}, ...}` to the
  callback per element (mirror the existing multi-array `arrayfun`).
- String-name: map the legacy names MATLAB supports (`isempty`, `length`,
  `ndims`, `prodofsize`, `size`, `isreal`, `islogical`, `isclass`) to the
  corresponding per-cell operation. Moderate; `arrayfun` multi-array is the
  model for the multi-cell part.

## References
- `libs/builtin/src/language/cells/cell.cpp` (cellfun, cellfun_reg)
- MATLAB `doc cellfun`
