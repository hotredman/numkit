# signal.deconv — throws on integer / logical input

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (threw on common valid input; deconv pairs with conv)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (cycle 61 conv-sibling sweep; fixed c62)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 62),
  `libs/signal/src/convolution/convolution.cpp`. `deconv` now promotes
  integer/logical operands to double up front (reusing the
  `convPromoteToDouble` helper added for conv in cycle 61) before the
  `doubleData()` accessors. Both the quotient and the remainder are always
  double — MATLAB does NOT preserve the integer class for deconv (same as
  conv; unlike kron/cross). The promotion sits before the `na > nb`
  divisor-longer branch.
- **Lenient edge (documented, NOT MATLAB parity):** MATLAB ERRORS on the
  `na > nb` divisor-longer branch with INTEGER input ("Inputs must be
  floats, namely single or double" — the same `superiorfloat` quirk that
  makes `polyval` reject a vector integer x). Because numkit promotes before
  that branch, it stays lenient and returns double (q scalar 0, r =
  numerator). This is the directive's preferred stance (don't add errors to
  match MATLAB; cf. `dot`); the edge is intentionally not fingerprinted in
  the parity spec, and the gtest guards it as a deliberate lenient extension
  (`DivisorLongerIntegerIsLenient`). The normal `na <= nb` integer path —
  the actual fix — matches MATLAB exactly.
- Verified vs MATLAB R2025b: `deconv(int8([1 3 5 3]),int8([1 1]))`=double
  q=[1 2 3]; `deconv(int16([2 7 7 2]),int16([1 2]))`=q=[2 3 1];
  `deconv([1 3 5 3],int8([1 1]))`=q=[1 2 3] (mixed);
  `deconv(logical([1 0 1 0]),[1 1])`=q=[1 -1 2]; `[q,r]=deconv(int8(...))`
  → q,r both double, r all-zero on exact division;
  `deconv(int8([1 1]),int8([1 2 3]))` (na>nb) → q scalar 0, r=[1 1] double;
  `deconv([2 7 7 2],[1 2])`=q=[2 3 1] (unchanged).
- Live guard: `libs/signal/tests/deconv_integer_input_test.cpp` (6 TEST_F) +
  `SignalKnownBug.DeconvIntegerInput` (flipped live). Parity:
  `tools/parity/specs/deconv_integer_input.json` (correctness=OK). Smoke:
  `libs/signal/tests/smoke/deconv_integer_input_smoke.m`.

## Symptom
`deconv` threw "Not a double array" whenever either operand was an integer
or logical array. MATLAB R2025b accepts integer/logical input, promoting to
double and returning double quotient/remainder.

## Repro (numkit vs MATLAB R2025b)
```matlab
deconv(int8([1 3 5 3]), int8([1 1]))   % numkit: ERROR "Not a double array"
%                                        MATLAB: double [1 2 3]
[q,r] = deconv(int8([1 3 5 3]), int8([1 1]))  % MATLAB: q=[1 2 3] r=[0 0 0 0] (double)
deconv([1 3 5 3], int8([1 1]))         % numkit: ERROR; MATLAB: double [1 2 3]
```

## Root cause
`libs/signal/src/convolution/convolution.cpp` `deconv` read its operands via
`b.doubleData()` / `a.doubleData()`, which throw on any non-`DOUBLE` storage.
No integer/logical promotion existed.

## Related (catalogued, not part of this fix)
Same conv-sibling probe: `polyval` accepts a *scalar* integer x (→ double)
but ERRORS on a *vector* integer x ("Inputs must be floats" via
`superiorfloat`) — matching that scalar/vector split is a MATLAB quirk, not
a clean fix; `accumarray` rejects integer/logical vals (MATLAB → double);
`cross` throws on integer input (MATLAB preserves the int class with
intermediate per-product saturation — harder). `dot` is the reverse —
numkit lenient where MATLAB errors.

## Guard
`libs/signal/tests/known_bugs_test.cpp` → `SignalKnownBug.DeconvIntegerInput`
(live) plus the dedicated `deconv_integer_input_test.cpp`.

## References
- `libs/signal/src/convolution/convolution.cpp` (`deconv`, `convPromoteToDouble`)
- MATLAB `doc deconv` (integer inputs promoted to double)
