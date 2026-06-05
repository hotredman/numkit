# linalg.kron — integer class not preserved (returned double)

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (wrong output class on common valid input)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (cycle 60 — integer-class preservation,
  newly reachable since integer concatenation was fixed in cycle 59)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 60), `libs/linalg/src/vector_ops.cpp`.
  `kron` now keeps the integer class of integer operands. `kronIntegerClass`
  picks the output class (same-int-class → that class; integer + real scalar
  double → that integer class; otherwise double); element products are still
  computed in a double workspace via `elemAsDouble` (correct for every integer
  operand class) and finished with `narrowKronToInteger` (round-half-away +
  saturate, mirroring core's `castConcatToInteger`).
- Verified vs MATLAB R2025b: `kron(int8([1 2]),int8([1 1]))`=int8 [1 1 2 2];
  `kron(int8(100),int8(2))`=127 (sat hi); `kron(uint8(200),uint8(2))`=255;
  `kron(int8(-100),int8(2))`=-128 (sat lo); `kron(int8([2 3]),2)`=int8 [4 6]
  (scalar double cast); `kron(2,int8([2 3]))`=int8 [4 6]; `kron(int8(2),1.5)`=3
  (round-half-away); `kron([1 2],[3 4])`=double [3 4 6 8] (unchanged).
- Live guard: `libs/linalg/tests/kron_integer_class_test.cpp` (6 TEST_F) +
  `LinalgKnownBug.KronIntegerClass` (flipped live). Parity:
  `tools/parity/specs/kron_integer_class.json` (correctness=OK). Smoke:
  `libs/linalg/tests/smoke/kron_integer_class_smoke.m`.

## Symptom
`kron` of two integer arrays returned a `double` result. MATLAB R2025b
preserves the integer class (with saturating arithmetic): the Kronecker
product of two `int8` arrays is `int8`, of two `uint16` arrays is `uint16`,
etc. A single integer operand combined with a real scalar double also keeps
the integer class.

## Repro (numkit vs MATLAB R2025b)
```matlab
kron(int8([1 2]), int8([1 1]))   % numkit: double [1 1 2 2]
%                                  MATLAB: int8   [1 1 2 2]
kron(int8(100), int8(2))         % numkit: double 200
%                                  MATLAB: int8   127   (saturates)
kron(int8([2 3]), 2)             % numkit: double [4 6]
%                                  MATLAB: int8   [4 6] (scalar double cast)
```

## Root cause
`libs/linalg/src/vector_ops.cpp` `kron` always allocated a `DOUBLE` result
matrix and wrote `double` element products, ignoring the operand classes.

## MATLAB class rule (R2025b)
- both operands the same integer class → that class (saturating);
- integer + a real **scalar** double → the integer class (the scalar is cast,
  round-half-away then saturate);
- different integer classes, integer + a **non-scalar** double, and
  integer + logical all **ERROR** ("Integers can only be combined with
  integers of the same class, or scalar doubles"). numkit stays lenient on
  those (returns double); matching MATLAB's error there is a separate concern
  and is not part of this fix.

## Guard
`libs/linalg/tests/known_bugs_test.cpp` → `LinalgKnownBug.KronIntegerClass`
(live) plus the dedicated `kron_integer_class_test.cpp`.

## References
- `libs/linalg/src/vector_ops.cpp` (`kron`, `kronIntegerClass`,
  `narrowKronToInteger`)
- `core/src/value.cpp` (`castConcatToInteger` — the saturating-narrow pattern
  this mirrors)
- MATLAB `doc kron` + integer-class arithmetic rules
