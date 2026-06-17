# math.trapz — rejects logical input

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (throws on valid input MATLAB accepts)
- **Kind:** bug
- **Found:** 2026-06-05 (logical-input sweep — documented in
  bugs/lang/cumulative-logical.md "Related")

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 43),
  `src/math/src/integration/integration.cpp` (`trapz_reg`). The
  trapz dispatch read `doubleData()` on both the X and Y arguments, which
  throws "Not a double array" for LOGICAL storage. Added a logical→double
  promotion (`toDoubleValue`) for `args[0]` (X or Y) and `args[1]` (X) at the
  adapter entry, then dispatch on the promoted copies. The sibling `cumtrapz`
  already handled this via its `toDoubleCopy` reader; this brings `trapz` in
  line.
- **Result class = double** for all forms (MATLAB promotes logical for
  integration; the logical class is NOT preserved, unlike cummax/cummin).
- Only **logical** is coerced. char / other non-numeric still hit the
  `doubleData()` throw, which matches MATLAB ("numeric or logical" only).
  Complex / double / integer paths are untouched (zero regression — the
  promotion is gated on `isLogical()`).
- Verified vs MATLAB R2025b:
  `trapz(logical([1 0 1 1]))`=`2`;
  `trapz([1 3 4 7], logical([1 0 1 1]))`=`4.5`;
  `trapz(logical([0 1 1 1]), [1 2 3 4])`=`1.5` (logical X promoted too);
  `trapz(logical([1 0; 1 1]))`=`[1 0.5]` (column-wise);
  `trapz(logical([1 0; 1 1]), 2)`=`[0.5; 1]`;
  `trapz(true)`=`0`; `trapz(logical([]))`=`0`.
- Live guard: `tests/math/trapz_logical_test.cpp` (5 TEST_F) +
  `BuiltinKnownBug.TrapzLogical` flipped live. Parity:
  `tools/parity/specs/trapz_logical.json` (correctness=OK). Smoke:
  `tests/math/smoke/trapz_logical_smoke.m`.

## Symptom
`trapz` throws on a `logical` X and/or Y; MATLAB accepts logical (as 0/1),
returning a double.

## Repro
```matlab
trapz(logical([1 0 1 1]))
% numkit: ERROR "Not a double array (in call to 'trapz')"
% MATLAB: 2   (class double)

trapz([1 3 4 7], logical([1 0 1 1]))   % MATLAB: 4.5
trapz(logical([0 1 1 1]), [1 2 3 4])   % MATLAB: 1.5 (logical X promoted)
```

## Root cause
`trapz_reg` and the public `trapz` read the input via `doubleData()`, which has
no buffer for logical storage and throws. (`cumtrapz` reads via the generic
`toDoubleCopy`, which is why it already accepted logical.)

## References
- `src/math/src/integration/integration.cpp` (`trapz_reg`)
- Related still-open: `sort(logical(...))` throws — MATLAB preserves the
  logical class (separate ticket). See bugs/lang/cumulative-logical.md.
- MATLAB `doc trapz`
