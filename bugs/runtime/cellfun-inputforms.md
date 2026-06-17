# runtime.cellfun — multiple cell arrays + string function-name forms unsupported

- **Status:** ✅ FIXED (2026-06-05)
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
`cellfun_reg` (`toolboxes/builtin/src/language/cells/cell.cpp`) accepts exactly
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

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 21),
  `toolboxes/builtin/src/language/cells/cell.cpp`.
- **Multi-cell:** `cellfun_reg` now collects all leading cell-array positional
  args (until the first option), requires equal sizes, and zips them through a
  new `cellfunN` helper that calls `fn(C1{i}, C2{i}, …)` per element via the
  engine handle (covers anonymous `@(a,b)…` and multi-arg builtins like
  `@plus`). `'UniformOutput'` is parsed at `1 + nCells`. The pausable
  state-machine path falls back to the synchronous reg when a second leading
  cell is present (one added `args[2].isCell()` guard).
- **String-name:** a new `cellfunStringForm` handles the legacy
  `cellfun('name', C[, extra])` forms: `isempty`/`isreal`/`islogical` →
  logical, `length`/`ndims`/`prodofsize` → double, `size`,C,k → `size(C{i},k)`,
  `isclass`,C,'cls' → `isa(C{i},'cls')`. An unrecognised name errors with a
  "pass a function handle" hint.
- Verified vs MATLAB R2025b: `cellfun(@(a,b)a+b,{1,2,3},{10,20,30})=[11 22 33]`,
  3-cell `[111 222]`, multi-cell `UniformOutput=false` → cell, all eight string
  names (incl. `size` dim and `isclass`). Single-cell + builtin-handle
  fast-paths unchanged.
- Live guard: `toolboxes/builtin/tests/cellfun_inputforms_test.cpp` (8 TEST_F) +
  flipped `BuiltinKnownBug.CellfunMultiCell` / `CellfunStringName` live. Parity:
  `tools/parity/specs/cellfun.json` (extended; correctness=OK). Smoke:
  `toolboxes/builtin/tests/smoke/cellfun_inputforms_smoke.m`.

## References
- `toolboxes/builtin/src/language/cells/cell.cpp` (cellfun, cellfun_reg)
- MATLAB `doc cellfun`
