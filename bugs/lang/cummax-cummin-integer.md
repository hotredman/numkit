# lang.cummax/cummin — reject integer input

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P3 (throws on valid input MATLAB accepts; narrower than the
  logical/char gaps)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (type-input sweep continuation)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 48),
  `src/lang/src/arrays/matrix.cpp` (`cummax` / `cummin`). c44
  added a LOGICAL branch to these but no integer branch, so an integer input
  still reached the double-only `cumScanDispatch` → `doubleData()` → "Not a
  double array". (cumsum/cumprod already had `cumIntegerNative`; cummax/cummin
  did not.)
- MATLAB PRESERVES the integer class for cummax/cummin (they are order
  statistics — the running max/min is always one of the input elements, so no
  saturation or precision loss). Fix: an integer branch mirroring the logical
  one — promote to double (`toDoubleValue`), run the double cummax/cummin, then
  narrow back to the original integer class with `doubleToIntegerExact`. The
  round-trip is exact because the result is a subset of the (in-range) input.
- 'reverse' / dim flags flow through unchanged. double / logical / complex
  paths untouched (zero regression — gated on `isIntegerType`).
- Verified vs MATLAB R2025b:
  `cummax(int8([3 1 2 5 4]))`=`[3 3 3 5 5]` int8;
  `cummin(int8([3 1 2 5 4]))`=`[3 1 1 1 1]` int8;
  `cummax(uint16([30 10 50 20]))`=uint16; 2-D dim2 `cummax(int8([3 1;1 5]),2)`
  =`[3 3;1 5]`; `'reverse'` `[5 5 5 5 4]`; negatives `cummin(int8([0 -3 2 -5]))`
  =`[0 -3 -3 -5]`.
- Live guard: `tests/lang/cummax_cummin_integer_test.cpp` (5 TEST_F) +
  `BuiltinKnownBug.CummaxCumminInteger` flipped live. Parity:
  `tools/parity/specs/cummax_cummin_integer.json` (correctness=OK). Smoke:
  `tests/lang/smoke/cummax_cummin_integer_smoke.m`.

## Symptom
`cummax` / `cummin` throw on an integer array; MATLAB returns the running
max/min in the SAME integer class.

## Repro
```matlab
cummax(int8([3 1 2 5 4]))   % numkit: ERROR "Not a double array"; MATLAB: int8 [3 3 3 5 5]
cummin(int8([3 1 2 5 4]))   % numkit: ERROR;                      MATLAB: int8 [3 1 1 1 1]
```

## Root cause
cummax/cummin route everything except logical into the double-only
`cumScanDispatch`, which reads `doubleData()` and throws for integer storage.

## References
- `src/lang/src/arrays/matrix.cpp` (`cummax`, `cummin`)
- Related still-open (separate, larger): the mov* family (movmax/movmin/
  movsum/movmean) is also double-only and throws on logical/integer.
- MATLAB `doc cummax`, `doc cummin`
