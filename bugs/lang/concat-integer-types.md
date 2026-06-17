# concat — integer types rejected (cat / [;] / [,] / vertcat / horzcat)

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (threw on common valid input; integer arrays are widely used)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (cycle 53)
- **Scope:** CORE (fixed under user approval in cycle 59)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 59, user-approved core change),
  `core/src/value.cpp`. `promoteNumericType` now returns the class of the FIRST
  integer operand when any operand is integer (MATLAB R2025b dominance — it is
  NOT an error, even for mixed integer classes or integer+complex). `horzcat` /
  `vertcat` compute an integer result in a DOUBLE workspace (the existing
  `elemAsDouble` copy reads every operand class incl. the real part of complex)
  and finish with `castConcatToInteger` (round-half-away + saturate). The N-D
  `cat(3,...)` path inherits the fix.
- Verified vs MATLAB R2025b (re-probed — R2025b is more permissive than the
  original ticket assumed: mixed int classes and int+complex do NOT error):
  `[int8;int8]`=int8 [1 2;3 4]; `[int8,int8]`=int8 1x4; `cat(1,uint16)`=uint16;
  `cat(3,int8,int8)`=int8 1x2x2; `[int8(5);50.6]`=51 (round); `[int8(5);300]`=127
  (sat hi); `[int8(5);-300]`=-128 (sat lo); `[int8(5);true]`=1;
  `[int8,int16]`=int8 (first wins); `[int16,int8]`=int16; `[int8;2+3i]`=2
  (real part). double / logical / complex / char concat unchanged (zero
  regression; full suite 11575/11575).
- Live guard: `tests/lang/concat_integer_types_test.cpp` (5 TEST_F) +
  `BuiltinKnownBug.ConcatIntegerTypes` flipped live. Parity:
  `tools/parity/specs/concat_integer_types.json` (correctness=OK). Smoke:
  `tests/lang/smoke/concat_integer_types_smoke.m`.

## Symptom
Concatenating integer-typed arrays throws "Concatenation not supported for
type '<int>'". MATLAB concatenates integers, preserving the integer class;
a mixed integer+double concatenation yields the integer class (the double
operand is cast). Affects EVERY concat path: `cat(1/2/3,...)`, the `[;]` /
`[,]` matrix-construction operators, `vertcat`, `horzcat`.

## Repro (numkit vs MATLAB R2025b)
```matlab
cat(1, int8([1 2]), int8([3 4]))   % numkit: ERROR "Concatenation not supported for type 'int8'"
%                                    MATLAB: int8 [1 2; 3 4]
[int8([1 2]); int8([3 4])]         % numkit: ERROR "... (in matrix construction)"
%                                    MATLAB: int8 [1 2; 3 4]
[int8([1 2]), int8([3 4])]         % MATLAB: int8 [1 2 3 4]  (1x4)
cat(1, uint16([10 20]), uint16([30 40]))   % MATLAB: uint16 [10 20; 30 40]
cat(1, int8([1 2]), [3 4])         % MATLAB: int8 [1 2; 3 4]  (double cast to int8)
vertcat(int8(1), int8(2))          % MATLAB: int8 [1; 2]
horzcat(int8(5), int8(6))          % MATLAB: int8 [5 6]
```

## Root cause
`core/src/value.cpp` `promoteNumericType` (≈ line 497) handles only
LOGICAL / DOUBLE / COMPLEX and throws in the `default:` branch for every other
type. Its own comment is now stale: "Integer types (INT8..UINT64) are declared
but not yet creatable at runtime; when they are, add them here with
appropriate promotion rules." Integer arrays ARE creatable at runtime now
(e.g. `int8([1 2])`, `colonRangeTyped` already writes all integer classes), so
the concat promotion table needs the integer rules + the element-copy paths
need integer storage handling.

## Suggested fix (CORE — needs supervision)
1. Extend `promoteNumericType` with MATLAB's class-dominance rules: all-same
   integer class → that class; integer + double/logical → that integer class
   (cast the others); integer + a DIFFERENT integer class → MATLAB errors
   ("Concatenation of different integer types is not allowed"); integer +
   complex → MATLAB errors. char stays its own existing path.
2. Extend the concat element-copy to write the chosen integer storage
   (saturating cast from each operand via the typed accessors, mirroring
   `colonRangeTyped`'s per-type `write_loop`).
3. Verify both the builtin paths (cat/vertcat/horzcat in
   `src/lang/src/arrays/`) and the VM/tree-walker matrix-construction
   opcode route through the fixed primitive.

## Guard
`tests/mixed/known_bugs_test.cpp` → `DISABLED_ConcatIntegerTypes`
(asserts the MATLAB-correct int8 concatenation; flip the prefix when fixed).

## References
- `core/src/value.cpp` (`promoteNumericType` + concat element-copy)
- MATLAB `doc cat` (integer class preservation + mixed-class rules)
