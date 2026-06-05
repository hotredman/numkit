# builtin.cumsum/cumprod/cummax/cummin — reject logical input

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (throws on valid input MATLAB accepts)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (logical-input handling sweep)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 42),
  `libs/builtin/src/language/arrays/matrix.cpp`. The public `cumsum`,
  `cumprod`, `cummax`, `cummin` branched on `isIntegerType` and `COMPLEX` but
  let LOGICAL fall through to the DOUBLE path, whose `doubleData()` throws
  "Not a double array". Added a logical branch to each: promote with
  `toDoubleValue` and run the existing double kernel.
- **Class rule matches MATLAB exactly** (verified R2025b):
  `cumsum`/`cumprod` PROMOTE logical→**double**; `cummax`/`cummin` PRESERVE the
  **logical** class. cummax/cummin therefore narrow the 0/1 double result back
  to LOGICAL via a small `logicalizeCumResult` helper (mirrors `xorOf`'s
  double→logical narrowing; scalar handled via `Value::logicalScalar`).
- `char` input still errors on all four — MATLAB also errors
  (`cumsum('abc')` → "Invalid data type. First input argument must be numeric
  or logical."), so the existing doubleData() throw for char is correct and
  left untouched.
- 'reverse' / 'omitnan' flags + explicit dim all flow through unchanged
  (the `_reg` flag wrappers flip/scan the logical array, then the public fn
  promotes); omitnan is a no-op for logical (no NaN).
- Verified vs MATLAB R2025b: `cumsum(logical([1 0 1 1]))`=`[1 1 2 3]` double;
  `cumprod(logical([1 1 0 1]))`=`[1 1 0 0]` double;
  `cummax(logical([0 1 0 1]))`=`[0 1 1 1]` logical;
  `cummin(logical([1 1 0 1]))`=`[1 1 0 0]` logical;
  2-D column/dim2, scalar `cumsum(true)`=`1` double.
- Live guard: `libs/builtin/tests/cumulative_logical_test.cpp` (6 TEST_F) +
  `BuiltinKnownBug.CumulativeLogical` flipped live. Parity:
  `tools/parity/specs/cumulative_logical.json` (correctness=OK). Smoke:
  `libs/builtin/tests/smoke/cumulative_logical_smoke.m`.

## Symptom
`cumsum`/`cumprod`/`cummax`/`cummin` throw on a `logical` array; MATLAB accepts
logical (treating it as 0/1) for all four.

## Repro
```matlab
cumsum(logical([1 0 1 1]))
% numkit: ERROR "Not a double array (in call to 'cumsum')"
% MATLAB: [1 1 2 3]   (class double)

cummax(logical([0 1 0 1]))
% numkit: ERROR "Not a double array (in call to 'cummax')"
% MATLAB: [0 1 1 1]   (class LOGICAL — preserved)

cumprod(logical([1 1 0 1]))   % MATLAB: [1 1 0 0] double
cummin(logical([1 1 0 1]))    % MATLAB: [1 1 0 0] logical
```

## Root cause
The type dispatch in each public function handled DOUBLE / integer / COMPLEX
but not LOGICAL, so logical input reached `doubleData()` (which has no buffer
for logical storage) and threw.

## Related (NOT fixed here — separate, lower-priority gaps)
The same logical-input sweep mapped the rest of the surface (all re-verified
2026-06-05). Most already work; two more throw and are left for a future cycle
(distinct functions in other files, each with their own multi-form complexity):
- **`sort(logical(...))` throws** — MATLAB returns **logical** `[0 0 1 1]`
  (class preserved). Needs class-preserving coercion across the index-output /
  `'descend'` / dim forms. `libs/builtin/.../sort`.
- **`trapz(logical(...))` throws** — MATLAB returns **double** `2`. Needs
  promotion at the `trapz(Y)` / `trapz(X,Y)` / dim entry.
  `libs/builtin/src/math/integration/integration.cpp`.
- Already correct (no fix needed): `max`/`min`/`mode` (return the logical
  class — guarded by `ReductionDimTest.MaxLogicalReturnsLogical` /
  `ModeLogicalReturnsLogical` in stats_test.cpp), `cumtrapz`, and
  `sum`/`prod`/`mean`/`median`/`diff`/`var`/`std` (generic `elemAsDouble`
  reader). The earlier "max/min/mode segfault" note was a mis-probe — they do
  not crash.

## References
- `libs/builtin/src/language/arrays/matrix.cpp` (`cumsum` ×2, `cumprod`,
  `cummax`, `cummin`)
- MATLAB `doc cumsum`, `doc cummax`
