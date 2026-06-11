# linalg.cross — throws on integer input (should preserve int class)

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (threw on common valid input; cross of integer vectors)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (cycle 61 integer-input sweep; fixed c64)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 64),
  `toolboxes/linalg/src/vector_ops.cpp`. `crossCore` now keeps the integer class of
  integer operands using MATLAB's **per-operation saturating integer
  arithmetic** (`crossIntegerSaturating`): each element product saturates to
  the int range BEFORE the component subtraction, and the subtraction
  saturates too. The output class is picked by `crossIntegerClass`
  (same-int-class → that class; integer + a real double of ANY shape → that
  integer class — each product is an int-scalar × double-scalar, an allowed
  mix). Lenient mixes (different int classes, integer + logical) compute in
  double via `crossToDouble`.
- Verified vs MATLAB R2025b: `cross(int8([1 2 3]),int8([4 5 6]))`=int8
  [-3 6 -3]; `cross(int32([1 0 0]),int32([0 1 0]))`=int32 [0 0 1];
  **`cross(int8([100 100 0]),int8([0 100 100]))`=int8 [127 -127 127]** (the
  100·100 product saturates to 127 before the subtraction → comp2 = 0-127 =
  -127, NOT -128 that compute-in-double-then-narrow would give);
  `cross(uint8([1 2 3]),uint8([4 5 6]))`=uint8 [0 6 0] (negatives clamp to 0);
  `cross(int8([1 2 3]),[4 5 6])`=int8 [-3 6 -3] (int + double); int16
  [-300 600 -300]; Nx3 layout; double*double unchanged [0 0 1].
- Live guard: `toolboxes/linalg/tests/cross_integer_class_test.cpp` (6 TEST_F) +
  `LinalgKnownBug.CrossIntegerClass` (flipped live). Parity:
  `tools/parity/specs/cross_integer_class.json` (correctness=OK). Smoke:
  `toolboxes/linalg/tests/smoke/cross_integer_class_smoke.m`.

## Symptom
`cross` threw "Not a double array" whenever either operand was an integer
array. MATLAB R2025b accepts integer cross products and preserves the integer
class with saturating arithmetic.

## Repro (numkit vs MATLAB R2025b)
```matlab
cross(int8([1 2 3]), int8([4 5 6]))            % numkit: ERROR "Not a double array"
%                                                MATLAB: int8 [-3 6 -3]
cross(int8([100 100 0]), int8([0 100 100]))    % MATLAB: int8 [127 -127 127]
%                                                (per-op saturation, not -128)
cross(int8([1 2 3]), [4 5 6])                  % MATLAB: int8 [-3 6 -3] (int + double)
```

## Root cause
`toolboxes/linalg/src/vector_ops.cpp` `crossCore` (real path) read its operands
via `a.doubleData()` / `b.doubleData()`, which throw on non-`DOUBLE` storage,
and always allocated a DOUBLE result.

## Why this is harder than kron/conv
kron narrows once at the end (compute-in-double, narrow). cross can't — MATLAB
applies saturation at EACH product and subtraction, so `100*100` saturates to
127 before `0 - 127`. Computing the whole component in double (`0 - 10000 =
-10000`) then narrowing gives -128, the wrong answer. `crossIntegerSaturating`
saturates per operation to match.

## Lenient niches (documented, NOT MATLAB parity)
- **different integer classes** (`cross(int8, int16)`) and **integer +
  logical**: MATLAB ERRORS ("Integers can only be combined with integers of
  the same class, or scalar doubles"); numkit computes in double. Rare; left
  lenient (cf. `dot`, `deconv` na>nb edge). Not fingerprinted.

## Guard
`toolboxes/linalg/tests/known_bugs_test.cpp` → `LinalgKnownBug.CrossIntegerClass`
(live) plus the dedicated `cross_integer_class_test.cpp`.

## References
- `toolboxes/linalg/src/vector_ops.cpp` (`crossCore`, `crossIntegerClass`,
  `crossIntegerSaturating`, `intTypeRange`, `narrowKronToInteger`)
- MATLAB `doc cross` + integer-class saturating arithmetic
