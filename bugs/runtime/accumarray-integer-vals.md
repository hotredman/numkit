# accumarray — throws on integer / logical vals

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (threw on common valid input; accumulating integer data is common)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (cycle 61 conv-sibling sweep; fixed c63)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 63),
  `src/runtime/src/language/arrays/accum.cpp`. `accumarray_reg` now promotes
  integer/logical `vals` to double (`toDoubleValue`) before dispatch, so the
  double-only inner loops are valid. The output class follows the reducer,
  matching MATLAB: `sum`/`prod`/`mean` → double, but `max`/`min` PRESERVE the
  integer class (the promoted-double result is narrowed back with
  `doubleToIntegerExact`, exact for the order statistics).
- Verified vs MATLAB R2025b: default sum int8 → double [40 20]; `@max` int8 →
  int8 [100 100]; `@min` int8 → int8 [10 20]; `@max` uint16 → uint16 [100 200];
  `@prod` → double [300 20]; `@mean` → double [20 20]; 2-D subs [5 0 0 7];
  sz [3 1] → [40 20 0]; fillval -1 → [5 -1 7 -1]; logical default-sum → double
  [2 0]; double vals unchanged [40 20].
- Live guard: `tests/builtin/accumarray_integer_vals_test.cpp` (6 TEST_F)
  + `BuiltinKnownBug.AccumarrayIntegerVals` (flipped live). Parity:
  `tools/parity/specs/accumarray_integer_vals.json` (correctness=OK). Smoke:
  `tests/builtin/smoke/accumarray_integer_vals_smoke.m`.

## Symptom
`accumarray` threw "accumarray: vals must be DOUBLE" whenever `vals` was an
integer or logical array. MATLAB R2025b accepts them and accumulates,
returning double for sum/prod/mean and the preserved integer class for
max/min.

## Repro (numkit vs MATLAB R2025b)
```matlab
accumarray([1;2;1], int8([10;20;30]))            % numkit: ERROR "vals must be DOUBLE"
%                                                  MATLAB: double [40 20]
accumarray([1;2;1], int8([100;100;30]), [], @max) % numkit: ERROR
%                                                   MATLAB: int8 [100 100]
accumarray([1;2;1], logical([1;0;1]))            % numkit: ERROR; MATLAB: double [2 0]
```

## Root cause
`src/runtime/src/language/arrays/accum.cpp` checked `vals.type() != DOUBLE`
and threw in both inner functions (`accumarray`, `accumarrayGeneral`); the
inner loops read `vals.doubleData()`, which only exists for DOUBLE storage.

## Lenient niches (documented, NOT MATLAB parity)
- **logical + `@max`/`@min`:** MATLAB returns **logical** (`max`/`min` of a
  logical group is logical); numkit returns double (the int-class preserve is
  gated on `isIntegerType`, not logical). Rare; left lenient.
- **custom non-built-in handle** (e.g. `@(x)x(1)`): MATLAB passes the
  original-class group to the handle, so the output follows the handle's class
  (int8 in/int8 out); numkit applies the handle to a promoted double group, so
  it returns double. The recognised reducers (`sum`/`max`/`min`/`prod`/`mean`)
  take the typed built-in path and are unaffected. Rare; left lenient.

Both follow the directive's stance — don't add errors / extra class machinery
for rare niches; cf. the `deconv` na>nb edge and `dot`.

## Guard
`tests/builtin/known_bugs_test.cpp` → `BuiltinKnownBug.AccumarrayIntegerVals`
(live) plus the dedicated `accumarray_integer_vals_test.cpp`.

## References
- `src/runtime/src/language/arrays/accum.cpp` (`accumarray_reg`, `toDoubleValue`,
  `doubleToIntegerExact`)
- MATLAB `doc accumarray` (val type + per-reducer output class)
