# signal.conv — throws on integer / logical input

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (threw on common valid input; integer signals are widely used)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (cycle 61 — integer-input reach,
  newly relevant since integer arrays became usable in cycle 59)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 61),
  `libs/signal/src/convolution/convolution.cpp`. `conv` now promotes
  integer/logical operands to double up front (`convPromoteToDouble` via
  `elemAsDouble`) before the real fast-path's `doubleData()` accessors. The
  complex path already used `elemAsDouble`, so it was unaffected. The result
  is always double — MATLAB does NOT preserve the integer class for conv
  (unlike kron/cross).
- Verified vs MATLAB R2025b: `conv(int8([1 2 3]),int8([1 1]))`=double
  [1 3 5 3]; `'same'`=[3 5 3]; `'valid'`=[3 5]; `conv(int8,[1 1])`=double
  [1 3 5 3] (int+double); `conv(uint8,uint8)`=[1 3 5 3];
  `conv(int16([100 200]),int16([2 2]))`=[200 600 400];
  `conv(logical([1 0 1]),[1 1])`=double [1 1 1 1]; `conv([1 2 3],[4 5 6])`=
  double [4 13 28 27 18] (unchanged).
- Live guard: `libs/signal/tests/conv_integer_input_test.cpp` (6 TEST_F) +
  `SignalKnownBug.ConvIntegerInput` (flipped live). Parity:
  `tools/parity/specs/conv_integer_input.json` (correctness=OK). Smoke:
  `libs/signal/tests/smoke/conv_integer_input_smoke.m`.

## Symptom
`conv` threw "Not a double array" whenever either operand was an integer or
logical array. MATLAB R2025b accepts integer/logical input, promoting to
double and returning a double result (every shape: 'full' / 'same' /
'valid').

## Repro (numkit vs MATLAB R2025b)
```matlab
conv(int8([1 2 3]), int8([1 1]))   % numkit: ERROR "Not a double array"
%                                    MATLAB: double [1 3 5 3]
conv(int8([1 2 3]), [1 1])         % numkit: ERROR
%                                    MATLAB: double [1 3 5 3]
conv(logical([1 0 1]), [1 1])      % numkit: ERROR
%                                    MATLAB: double [1 1 1 1]
```

## Root cause
`libs/signal/src/convolution/convolution.cpp` `conv` (real path) read its
operands via `a.doubleData()` / `b.doubleData()`, which throw on any
non-`DOUBLE` storage. No integer/logical promotion existed (only the complex
path used the class-agnostic `elemAsDouble`).

## Related / not part of this fix
Sibling functions surfaced in the same probe (catalogued separately if/when
fixed): `polyval` and `deconv` likewise throw on integer input (MATLAB →
double); `cross` throws on integer input (MATLAB preserves the integer
class with intermediate per-product saturation — a harder fix);
`accumarray` rejects integer vals (MATLAB → double). `dot` is the reverse —
numkit is lenient (returns double) where MATLAB errors ("A and B must be
single or double").

## Guard
`libs/signal/tests/known_bugs_test.cpp` → `SignalKnownBug.ConvIntegerInput`
(live) plus the dedicated `conv_integer_input_test.cpp`.

## References
- `libs/signal/src/convolution/convolution.cpp` (`conv`, `convPromoteToDouble`)
- MATLAB `doc conv` (integer inputs promoted to double)
