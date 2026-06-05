# builtin.ismember/intersect/setdiff/union — reject char/logical/integer

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (throws on valid input MATLAB accepts; setops are common)
- **Kind:** bug
- **Found:** 2026-06-05 (type-input sweep; flagged in bugs/builtin/unique-typeclass.md)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 47),
  `libs/builtin/src/math/discrete/discrete.cpp`. The set-operation machinery
  (`setUnion`/`setIntersect`/`setDiff`/`ismember`/`setOpRows`/`emitSetopIndices`)
  is DOUBLE-only and reads `doubleData()`, so a char / logical / integer
  operand threw "Not a double array".
- MATLAB accepts all of these:
  - `intersect`/`setdiff`/`union` PRESERVE the input class on the VALUES output
    (the `ia`/`ib` index outputs stay double);
  - `ismember` returns a logical `tf` + double `loc`, so no value-narrow is
    needed — only the operands need promoting.
- Fix: at each reg entry, promote a char/logical/integer operand to a double
  working copy (`setopPromote` → `toDoubleValue`), run the existing logic on
  the promoted operands, and — for the three value-setops — narrow `outs[0]`
  back to the shared class (`narrowUniqueClass`, reused from unique). The narrow
  runs AFTER `emitSetopIndices` (which needs the double VALUES + double operands
  to compute the indices). The narrow class is the common class only when BOTH
  operands share the same char/logical/integer class; mixed classes (or any
  double) stay double (`setopNarrowClass`), matching MATLAB's promote-to-double.
- 'rows' / 'stable' / 'first'/'last' flags (scanned on the original `args`) and
  the index/`'rows'`-not-yet-supported behaviour are all unchanged. double and
  complex paths are untouched (zero regression — the promote is gated on
  char/logical/integer).
- Verified vs MATLAB R2025b:
  `ismember('b','abcd')`=1; `[tf,loc]=ismember('xbq','abcd')` → tf=`[0 1 0]`
  loc=`[0 2 0]`; `intersect('cabc','bdc')`=`'bc'` char, ia=`[3 1]` ib=`[1 3]`;
  `setdiff('abce','bd')`=`'ace'`; `union('ab','bc')`=`'abc'`;
  `union('bca','db','stable')`=`'bcad'`;
  `intersect(logical([1 0 1]),logical([0 0 1]))`=`[0 1]` logical;
  `intersect(int8([3 1 2]),int8([2 4 1]))`=`int8 [1 2]`.
- Live guard: `libs/builtin/tests/setops_typeclass_test.cpp` (8 TEST_F) +
  `BuiltinKnownBug.SetopsTypeClass` flipped live. Parity:
  `tools/parity/specs/setops_typeclass.json` (correctness=OK). Smoke:
  `libs/builtin/tests/smoke/setops_typeclass_smoke.m`.

## Symptom
`ismember`/`intersect`/`setdiff`/`union` throw on char / logical / integer
input; MATLAB accepts them (the three value-setops preserve the class).

## Repro
```matlab
intersect('cabc','bdc')   % numkit: ERROR "Not a double array"; MATLAB: 'bc' (char)
ismember('b','abcd')      % numkit: ERROR;                      MATLAB: 1 (logical)
setdiff('abce','bd')      % numkit: ERROR;                      MATLAB: 'ace'
union('ab','bc')          % numkit: ERROR;                      MATLAB: 'abc'
intersect(int8([3 1 2]),int8([2 4 1]))   % MATLAB: int8 [1 2]
```

## Root cause
The setop reg functions passed operands straight into the double-only setop
machinery; no per-class handling existed (same root cause as the unique gap).

## References
- `libs/builtin/src/math/discrete/discrete.cpp`
  (`ismember_reg`/`union_reg`/`intersect_reg`/`setdiff_reg`, `setopPromote`,
  `setopNarrowClass`, `narrowUniqueClass`)
- MATLAB `doc ismember` / `doc intersect` / `doc setdiff` / `doc union`
