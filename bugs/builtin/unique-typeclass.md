# builtin.unique — rejects char / logical / integer input

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (throws on valid input MATLAB accepts; `unique` is very common)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (type-input sweep, continuing the
  sort/cum*/trapz logical+char theme)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 46),
  `toolboxes/builtin/src/math/discrete/discrete.cpp` (`unique_reg`). The unique
  machinery (`unique` / `uniqueWithIndices` / `uniqueRows*`) is DOUBLE-only and
  reads `doubleData()`, so any non-double/non-complex input threw "Not a double
  array". Unlike `sort` (which already had an integer path), `unique` had NO
  type handling at all — it threw on integer, logical AND char.
- MATLAB preserves the input class on the unique VALUES (the `ia`/`ic` index
  outputs are always double). Fix: promote a non-double input to a double
  working copy (`toDoubleValue`), run the existing unique logic (covers
  'sorted'/'stable'/'first'/'last'/'rows' + 1/2/3 outputs), then narrow the
  VALUES output `outs[0]` back to the original class via a new
  `narrowUniqueClass` helper (LOGICAL / CHAR / INT8..UINT64). Gated on
  `isIntegerType || LOGICAL || CHAR`; double and complex paths are untouched
  (zero regression). The narrow runs AFTER `orientUniqueVec` (which reorients
  via `doubleData()`), so the working value stays double until the final narrow.
- The promote→unique→narrow round-trips exactly (char codes, logical 0/1,
  in-range integers all representable in double).
- Verified vs MATLAB R2025b:
  `unique('cbabc')`=`'abc'` char, `[u,ia,ic]` → ia=`[3 2 1]` ic=`[3 2 1 2 3]`
  (double); `unique('cbabc','stable')`=`'cba'`;
  `unique(logical([1 0 1 1]))`=`[0 1]` logical;
  `unique(int8([3 1 3 2]))`=`int8 [1 2 3]`; column-vector + 'rows' preserved.
- Live guard: `toolboxes/builtin/tests/unique_typeclass_test.cpp` (6 TEST_F) +
  `BuiltinKnownBug.UniqueTypeClass` flipped live. Parity:
  `tools/parity/specs/unique_typeclass.json` (correctness=OK). Smoke:
  `toolboxes/builtin/tests/smoke/unique_typeclass_smoke.m`.

## Symptom
`unique` throws on a char / logical / integer array; MATLAB returns the unique
values in the SAME class (`ia`/`ic` stay double).

## Repro
```matlab
unique('cbabc')              % numkit: ERROR "Not a double array"; MATLAB: 'abc' (char)
unique(logical([1 0 1 1]))   % numkit: ERROR;                      MATLAB: [0 1] (logical)
unique(int8([3 1 3 2]))      % numkit: ERROR;                      MATLAB: int8 [1 2 3]
```

## Root cause
`unique_reg` passed the input straight into the double-only unique machinery
with no per-class handling, so `doubleData()` threw for any non-double type.

## Related (found in the same sweep — separate, NOT fixed here)
- **`max('abc')` / `min('abc')` return char**, but MATLAB returns **double**
  (`max('abc')`=99, `min('abc')`=97). numkit has a live test
  `ReductionDimTest.MaxCharReturnsChar` enshrining the (wrong) char result, so
  fixing this is a behavior change + test flip — its own ticket.
- **`ismember` / `intersect` / `setdiff` / `union` throw on char/logical** too
  (two-input setops; MATLAB preserves class). Larger surface (multi-output,
  'rows', 'stable') — separate future cycle.

## References
- `toolboxes/builtin/src/math/discrete/discrete.cpp` (`unique_reg`, `narrowUniqueClass`)
- MATLAB `doc unique`
