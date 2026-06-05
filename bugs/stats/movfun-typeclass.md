# stats.movsum/movprod/movmean — reject integer/logical input

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P3 (throws on valid input MATLAB accepts)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (type-input sweep continuation)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 49),
  `libs/stats/src/moving/moving.cpp`. The moving functions run through the
  double-only `movingDriverDim` (reads `doubleData()`), so an integer/logical
  input threw "Not a double array".
- MATLAB PROMOTES an integer/logical operand to **double** for the arithmetic
  moving functions: `movsum(int8(...))`, `movprod(...)`, `movmean(...)` all
  return double (the class is NOT preserved). Fix: a gated element-wise promote
  (`movPromoteIntLogical`) at the top of `movmean_impl`/`movsum_impl`/
  `movprod_impl` (the single choke point for both the engine adapters and the
  public C++ functions). double / single / complex pass through unchanged;
  **char is NOT promoted** — MATLAB errors ("First input must be double or
  single"), so the `doubleData()` throw correctly remains for char.
- Verified vs MATLAB R2025b (window 3, endpoints 'shrink'):
  `movsum(int8([3 1 2 5 4]),3)`=`[4 6 8 11 9]` double;
  `movprod(...)`=`[3 6 10 40 20]` double;
  `movmean(...)`=`[2 2 2.6667 3.6667 4.5]` double;
  `movsum(logical([1 0 1 1 0]),3)`=`[1 2 2 2 1]` double.
- Live guard: `libs/stats/tests/movfun_typeclass_test.cpp` (5 TEST_F) +
  `StatsKnownBug.MovfunTypeClass` flipped live. Parity:
  `tools/parity/specs/movfun_typeclass.json` (correctness=OK). Smoke:
  `libs/stats/tests/smoke/movfun_typeclass_smoke.m`.

## Symptom
`movsum` / `movprod` / `movmean` throw on an integer/logical array; MATLAB
accepts it, returning a double.

## Repro
```matlab
movsum(int8([3 1 2 5 4]), 3)        % numkit: ERROR "Not a double array"
%                                     MATLAB: [4 6 8 11 9]  (class double)
movmean(int8([3 1 2 5 4]), 3)       % MATLAB: [2 2 2.6667 3.6667 4.5] double
movsum(logical([1 0 1 1 0]), 3)     % MATLAB: [1 2 2 2 1] double
```

## Root cause
All mov* impls funnel through `movingDriverDim`, which reads `doubleData()`;
no per-class promotion existed for the integer/logical case.

## Related
- **`movmax` / `movmin` / `movmedian`** (the class-PRESERVING order statistics)
  also threw — now FIXED 2026-06-05 (c50) in `bugs/stats/movfun-order-stats.md`
  (movmax/movmin preserve int+logical; movmedian rounds int half-away,
  logical→double). Together these close the mov* type-class sweep.
- `movstd` / `movvar` correctly reject integer input — MATLAB also errors
  ("First input must be double or single"). Left as-is.

## References
- `libs/stats/src/moving/moving.cpp` (`movmean_impl`/`movsum_impl`/
  `movprod_impl`, `movPromoteIntLogical`)
- MATLAB `doc movsum` / `doc movmean`
