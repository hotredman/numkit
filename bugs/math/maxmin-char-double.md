# math.max/min — return char for char input (should be double)

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (wrong output CLASS for char input; common reduction)
- **Kind:** bug
- **Found:** 2026-06-05 (type-class sweep follow-up; the documented behavior-change lead)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 52),
  `toolboxes/builtin/src/math/arithmetic/reductions.cpp` (`max_reg` / `min_reg`).
  The min/max reducers preserved the CHAR class (returning a char), but MATLAB
  returns **double** — the code point — for `max`/`min` of a char array (the
  char class is NOT preserved). Note `mode` DOES keep the char class — that is
  correct and untouched.
- Fix: promote a char operand to double (`toDoubleValue`) at the `max_reg` /
  `min_reg` adapter entry, so every MATLAB-supported form yields a double result
  — reduction `max(s)`, `max(s,[],dim)`, `max(s,[],'all')`, and the 2nd-output
  index. Non-char inputs are untouched (integer max/min still preserve their
  class; this is gated on `isChar()`), zero regression.
- Verified vs MATLAB R2025b:
  `max('abc')`=`99` double (`ischar`=0); `min('abc')`=`97`;
  `[v,i]=max('abc')` → v=99 i=3 (double); column-wise `max(['abc';'xyz'])`
  =`[120 121 122]` double; dim-2 `[99 122]`; `'all'`=99; `mode('abc')`=`'a'`
  (CHAR — unchanged, correct).
- NOTE: the binary `max('a','c')` form is a MATLAB ERROR ("Invalid second
  argument") — numkit is lenient (the a0/a1 promotion returns a double) but
  that form is NOT MATLAB-matched and is not asserted in the artefacts.
- The existing live test `ReductionDimTest.MaxCharReturnsChar` enshrined the
  wrong char result — renamed to `MaxCharReturnsDouble` and flipped to assert
  double (same commit). No internal code relied on max/min(char)→char (grep
  clean). `ModeCharReturnsChar` is left as-is (mode keeps char, per MATLAB).
- Live guard: `toolboxes/builtin/tests/maxmin_char_double_test.cpp` (5 TEST_F) +
  `BuiltinKnownBug.MaxMinCharDouble` flipped live + the flipped
  `ReductionDimTest.MaxCharReturnsDouble`. Parity:
  `tools/parity/specs/maxmin_char_double.json` (correctness=OK). Smoke:
  `toolboxes/builtin/tests/smoke/maxmin_char_double_smoke.m`.

## Symptom
`max`/`min` of a char array return a char; MATLAB returns a double (the code
point).

## Repro
```matlab
max('abc')          % numkit: 'c' (char);  MATLAB: 99 (double)
min('abc')          % numkit: 'a' (char);  MATLAB: 97 (double)
[v,i] = max('abc')  % MATLAB: v=99 (double), i=3
mode('abc')         % 'a' (char) on BOTH — mode preserves char (correct)
```

## Root cause
`dispatchMinMaxAll`/`dispatchMinMaxAlongDim` had a CHAR case with
`outType=ValueType::CHAR`, preserving the char class. MATLAB promotes char to
double for max/min (only mode keeps char).

## References
- `toolboxes/builtin/src/math/arithmetic/reductions.cpp` (`max_reg`, `min_reg`)
- MATLAB `doc max` / `doc min` (char inputs are converted to double)
