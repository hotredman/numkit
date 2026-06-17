# lang.sort — rejects char input

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (throws on valid input MATLAB accepts)
- **Kind:** bug
- **Found:** 2026-06-05 (while fixing sort-logical.md, c44 — char throws too)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 45),
  `toolboxes/builtin/src/language/arrays/matrix.cpp` (`sort_reg`). sort routed char
  through its DOUBLE path, whose `doubleData()` throws "Not a double array".
  MATLAB sorts char by CODE POINT and PRESERVES the char class on the values;
  the optional 2nd-output index stays double.
- Fix: a char branch in `sort_reg` next to the integer + logical branches —
  `copyToDouble` (char code points → double) → `sort` (order + indices
  identical) → narrow the sorted code points back to CHAR via a new
  `charizeSortResult` helper; the index output stays **double**, gated on
  `isChar()`.
- **Why a new narrow** (not `toChar`): `toChar` routes through
  `Value::fromString`, which FLATTENS a matrix to a row — wrong for 2-D char
  sort. `charizeSortResult` uses `createLike(d, CHAR)` (preserves 2-D / N-D
  dims) + `charDataMut()`, column-major layout matching the double buffer 1:1.
- Stability matches MATLAB (the double sort is stable): `[S,I]=sort('cbacb')`
  → `S='abbcc'`, `I=[3 2 5 1 4]`.
- Only **char** is coerced. 'string' (string arrays) sort through a different
  path and are untouched; logical / integer / double / complex unaffected
  (zero regression).
- Verified vs MATLAB R2025b:
  `sort('dcba')`=`'abcd'` char, `[S,I]` index `[4 3 2 1]` double;
  `sort('dcba','descend')`=`'dcba'`;
  2-D `sort(['bd';'ca'])`=`['ba';'cd']`, dim2 `sort(['bd';'ca'],2)`=`['bd';'ac']`;
  scalar `sort('x')`=`'x'`; empty `sort('')`=`''` (char, numel 0).
- Live guard: `toolboxes/builtin/tests/sort_char_test.cpp` (6 TEST_F) +
  `BuiltinKnownBug.SortChar` flipped live. Parity:
  `tools/parity/specs/sort_char.json` (correctness=OK). Smoke:
  `toolboxes/builtin/tests/smoke/sort_char_smoke.m`.

## Symptom
`sort` throws on a `char` array; MATLAB sorts it by code point preserving the
char class, with a double index for the 2nd output.

## Repro
```matlab
sort('dcba')
% numkit: ERROR "Not a double array (in call to 'sort')"
% MATLAB: 'abcd'   (class char)

[S, I] = sort('dcba')
% MATLAB: S = 'abcd' (char), I = [4 3 2 1] (double)
```

## Root cause
`sort_reg` had integer + logical branches (and a complex/string-aware generic
path) but no char branch, so char reached the DOUBLE `sort` overload's
`doubleData()` and threw.

## References
- `toolboxes/builtin/src/language/arrays/matrix.cpp` (`sort_reg`, `charizeSortResult`)
- Completes the type-class sweep for `sort` (double/int/logical/char all keep
  their class; index stays double). See bugs/lang/sort-logical.md.
- MATLAB `doc sort`
