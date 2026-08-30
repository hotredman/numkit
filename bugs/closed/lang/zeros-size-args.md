# lang — zeros/ones (and the array-creation family) mishandle edge-case size args

- **Status:** ✅ FIXED (2026-06-21) — `toDim()` clamp+validate in the shared dim parser
- **Severity:** P2 (negative dims crashed with bad_alloc; non-integer dims silently wrong)
- **Kind:** bug
- **Found:** 2026-06-21 (while working on the codegen subsystem — codegen's
  `static_cast<size_t>(dim)` had the same defect; surfaced the shared core gap)

## Symptom
The array-creation builtins that share the dimension parser
(`parseDimsArgs` / `parseDimsArgsND` in `numkit::ops` — `zeros`, `ones`,
`nan`, `inf`, `true`, `false`, `eye`, …) diverged from MATLAB R2025b on
edge-case size arguments:

```matlab
numel(zeros(-1, 3))   % numkit: ERROR "bad allocation (in call to 'zeros')"
                      % MATLAB:  0   (a 0x3 empty array)
zeros(2, 3, -1)       % numkit: bad_alloc
                      % MATLAB:  2x3x0 empty array
zeros(2.5)            % numkit: 2x2 (silently truncated)
                      % MATLAB:  ERROR "Size inputs must be integers."
zeros(1e300)          % numkit: 0x0 (UB cast of an out-of-range double -> 0)
                      % MATLAB:  ERROR (requested array too large)
```

## Root cause
The dim parser converted each `double` size argument with a bare
`static_cast<size_t>(d)` and no validation:
- a **negative** dim → a huge `size_t` (the float→unsigned conversion of a
  negative value is itself UB) → `bad_alloc` instead of MATLAB's clamp-to-0;
- a **non-integer** dim → truncated toward zero instead of MATLAB's error.

## Fix
Added `numkit::ops::toDim(double)` (in `src/ops/include/numkit/ops/helpers.hpp`)
and routed every dim conversion in `parseDimsArgs` + `parseDimsArgsND` through
it:
- non-integer or non-finite size → `throw std::runtime_error("Size inputs must
  be integers.")`;
- negative integer size → clamps to `0` (yielding an empty array, as MATLAB);
- size larger than `size_t` can hold → `throw` ("Requested array size is too
  large.").

Every guard precedes the cast, so a negative / NaN / Inf / out-of-range value
never reaches the (UB) float→unsigned conversion.

## Regression guard
`src/bundle/tests/known_bugs_test.cpp` — `BuiltinKnownBug.ZerosNegativeDimEmpty`
and `BuiltinKnownBug.ZerosNonIntegerDimThrows` (live, not DISABLED_).
