# concat — integer types rejected (cat / [;] / [,] / vertcat / horzcat)

- **Status:** 🔴 OPEN
- **Severity:** P2 (throws on common valid input; integer arrays are widely used)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (cycle 53)
- **Scope:** CORE (out of scope for the libs-only bug-fix loop — flagged for a
  supervised core-fix session)

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
   `libs/builtin/src/language/arrays/`) and the VM/tree-walker matrix-construction
   opcode route through the fixed primitive.

## Guard
`libs/builtin/tests/known_bugs_test.cpp` → `DISABLED_ConcatIntegerTypes`
(asserts the MATLAB-correct int8 concatenation; flip the prefix when fixed).

## References
- `core/src/value.cpp` (`promoteNumericType` + concat element-copy)
- MATLAB `doc cat` (integer class preservation + mixed-class rules)
