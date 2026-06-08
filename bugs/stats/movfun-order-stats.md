# stats.movmax/movmin/movmedian — reject integer/logical input

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P3 (throws on valid input MATLAB accepts)
- **Kind:** bug
- **Found:** 2026-06-05 (type-input sweep; the class-PRESERVING half of the
  mov* family, deferred from bugs/stats/movfun-typeclass.md)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 50),
  `toolboxes/stats/src/moving/moving.cpp` (`movmin_impl`/`movmax_impl`/
  `movmedian_impl`). Like the arithmetic mov* (c49), these ran through the
  double-only `movingDriverDim` and threw "Not a double array" on
  integer/logical — but unlike sum/prod/mean, MATLAB PRESERVES the class for
  these order statistics.
- Class rules (verified MATLAB R2025b), all now matched:
  - **movmax / movmin**: integer → same integer class (values are exact, a
    running max/min is one of the inputs); logical → **logical**.
  - **movmedian**: integer → same integer class with **round-half-away-from-zero**
    (a window median can be fractional: `int8` 1.5→2, 4.5→5, −1.5→−2); **logical
    → double** (a 0.5 median has no logical representation, so the class is NOT
    preserved here).
- Fix: promote int/logical → double (reuse `movPromoteIntLogical` from c49), run
  the existing driver, then narrow:
  - movmax/movmin int → `doubleToIntegerExact` (exact); logical → a local
    `movNarrowToLogical` (`!= 0`).
  - movmedian int → `std::round` into the double buffer (half-away-from-zero),
    then `doubleToIntegerExact` (which truncates, but the values are now exact
    rounded integers); logical/double → returned as double.
  double / complex paths untouched (zero regression). `movstd`/`movvar`
  correctly reject integer (MATLAB also errors) and are not touched.
- Verified vs MATLAB R2025b (window 3 unless noted, 'shrink'):
  `movmax(int8([3 1 2 5 4]),3)`=`[3 3 5 5 5]` int8;
  `movmin`=`[1 1 1 2 4]` int8;
  `movmedian(...,3)`=`[2 2 2 4 5]` int8; `movmedian(...,2)`=`[3 2 2 4 5]`;
  `movmedian(int8([-1 -2 -4 -5]),2)`=`[-1 -2 -3 -5]`;
  `movmax(logical([1 0 1 1 0]),3)`=`[1 1 1 1 1]` logical;
  `movmedian(logical([1 0 1 1 0]),3)`=`[0.5 1 1 1 0.5]` double.
- Live guard: `toolboxes/stats/tests/movfun_order_stats_test.cpp` (6 TEST_F) +
  `StatsKnownBug.MovfunOrderStats` flipped live. Parity:
  `tools/parity/specs/movfun_order_stats.json` (correctness=OK). Smoke:
  `toolboxes/stats/tests/smoke/movfun_order_stats_smoke.m`. This completes the mov*
  type-class sweep (with bugs/stats/movfun-typeclass.md for the arithmetic
  half).

## Symptom
`movmax` / `movmin` / `movmedian` throw on an integer/logical array; MATLAB
accepts it (preserving the class, except movmedian-on-logical → double).

## Repro
```matlab
movmax(int8([3 1 2 5 4]), 3)            % numkit: ERROR; MATLAB: int8 [3 3 5 5 5]
movmedian(int8([3 1 2 5 4]), 2)         % MATLAB: int8 [3 2 2 4 5] (1.5->2, 4.5->5)
movmax(logical([1 0 1 1 0]), 3)         % MATLAB: logical [1 1 1 1 1]
movmedian(logical([1 0 1 1 0]), 3)      % MATLAB: double [0.5 1 1 1 0.5]
```

## Root cause
All mov* impls funnel through `movingDriverDim` (reads `doubleData()`); no
per-class promotion/narrow existed for the order-statistic mov* functions.

## References
- `toolboxes/stats/src/moving/moving.cpp`
  (`movmin_impl`/`movmax_impl`/`movmedian_impl`, `movNarrowToLogical`,
  `movPromoteIntLogical`)
- Arithmetic half: bugs/stats/movfun-typeclass.md (c49)
- MATLAB `doc movmax` / `doc movmedian`
