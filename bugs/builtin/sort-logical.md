# builtin.sort — rejects logical input

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (throws on valid input MATLAB accepts)
- **Kind:** bug
- **Found:** 2026-06-05 (logical-input sweep — documented in
  bugs/builtin/cumulative-logical.md "Related")

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 44),
  `libs/builtin/src/language/arrays/matrix.cpp` (`sort_reg`). sort routed
  logical through its DOUBLE path, whose `doubleData()` throws "Not a double
  array". Added a logical branch that exactly mirrors the existing INTEGER
  branch: `copyToDouble` → `sort` (order + indices identical) → narrow the 0/1
  sorted VALUES back to LOGICAL via `logicalizeCumResult`; the optional 2nd
  output (index) stays **double**.
- **Class rule (verified MATLAB R2025b):** sorted VALUES PRESERVE the logical
  class (like the integer-class rule, unlike trapz/cumsum which promote to
  double); the index output is always double.
- Only **logical** is coerced (gated on `isLogical()`). double / integer /
  string / complex paths untouched (zero regression). sortrows is a separate
  adapter, unaffected.
- Stability matches MATLAB: equal elements keep input order, so the index of
  `sort(logical([0 1 0 1]))` is `[1 3 2 4]` (zeros first, original order), and
  `'descend'` of `logical([1 0 1 0 0])` gives index `[1 3 2 4 5]`.
- Verified vs MATLAB R2025b:
  `sort(logical([0 1 0 1]))`=`[0 0 1 1]` logical, idx `[1 3 2 4]`;
  `sort(...,'descend')`=`[1 1 0 0]`;
  2-D column-wise `sort(logical([1 0;0 1]))`=`[0 0;1 1]`;
  dim2 `sort(logical([1 0;0 1]),2)`=`[0 1;0 1]`;
  scalar `sort(true)`=`1` logical; `sort(logical([]))`=`[]`.
- Live guard: `libs/builtin/tests/sort_logical_test.cpp` (6 TEST_F) +
  `BuiltinKnownBug.SortLogical` flipped live. Parity:
  `tools/parity/specs/sort_logical.json` (correctness=OK). Smoke:
  `libs/builtin/tests/smoke/sort_logical_smoke.m`.

## Symptom
`sort` throws on a `logical` array; MATLAB sorts it (as 0/1) preserving the
logical class, with a double index for the 2nd output.

## Repro
```matlab
sort(logical([0 1 0 1]))
% numkit: ERROR "Not a double array (in call to 'sort')"
% MATLAB: [0 0 1 1]   (class logical)

[S, I] = sort(logical([0 1 0 1]))
% MATLAB: S = [0 0 1 1] (logical), I = [1 3 2 4] (double)
```

## Root cause
`sort_reg` had an integer-class branch and a complex/string-aware generic
branch but no logical branch, so logical reached the DOUBLE `sort` overload's
`doubleData()` and threw.

## Related (NOT fixed here — separate gap found 2026-06-05)
- **`sort` on a CHAR array also throws** "Not a double array" — MATLAB sorts
  char by code point and keeps the char class (`sort('dcba')`=`'abcd'`). This
  is a distinct pre-existing gap (not a regression from this fix; the logical
  branch is gated on `isLogical()` and does not touch char). Fixing it follows
  the same shape (copyToDouble → sort → narrow back to CHAR), but needs its own
  MATLAB probe (index/descend/2-D char) + a double→char narrow — left for a
  future cycle.

## References
- `libs/builtin/src/language/arrays/matrix.cpp` (`sort_reg`)
- This closes the documented `sort(logical)` entry of the logical-input sweep
  (bugs/builtin/cumulative-logical.md "Related").
- MATLAB `doc sort`
