# lang.cells — growing a cell with `x{end+1, 1} = value` grows the cell but LOSES the stored value

- **Status:** 🔴 OPEN
- **Severity:** P2 (the canonical append-to-cell idiom silently produces empty content)
- **Kind:** bug
- **Found:** 2026-08-31 while verifying the freqs freqint grid (the verification
  script built its case list with `cases{end+1,1} = ...` and every cell
  came back empty)

## Symptom

Auto-growing a cell by indexed assignment with `end+1` resizes the cell
but the assigned value never lands — reading the same index right after
gives empty.

## Repro (self-contained)

```matlab
clear;
x = {};
x{end+1, 1} = 42;
x{end+1, 1} = 'hi';
disp(x{1,1})
disp(class(x{2,1}))
% numkit:  (empty) ... class(x{1,1}) -> 'empty' / numel(x) == 2 but x{1,1} is []
% MATLAB R2025b: 42 / char
```

Also fails with `x{end+1} = ...` (linear index) — same lost value.

## Root cause (hypothesis)

The indexed-assignment path for cells sizes the target using `end`
BEFORE the value is written, then writes to the wrong (newly
zero-initialised) slot or drops the store outright — the grow path and
the store path disagree about the final index.

## Suggested fix

In the cell indexed-assignment: resolve `end` against the POST-growth
extent for the store target (or grow first, then store at the computed
index). Pin with a dual-engine test covering: empty-seed growth,
growth of a non-empty cell, growth by row vs column, and reading back
both the old and the newly appended content.

## References

- **Guard:** deferred — DISABLED_CellGrowthKeepsValue to be enabled in
  `src/bundle/tests/cell_struct_numtheory_batch_test.cpp` (this file
  documents the failure; the guard lands with the fix commit per the
  same pattern as the other engine bugs fixed today).
