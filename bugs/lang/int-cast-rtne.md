# Integer casts round ties-to-even on the SIMD path (should be half-away)

- **Status:** ✅ FIXED (2026-06-14)
- **Severity:** P2 (silent wrong values on a fundamental operation)
- **Kind:** bug
- **Found:** 2026-06-14 by the architecture/crutch audit (`casts_highway.cpp` comment flagged the risk; confirmed by probing the SIMD path).

## Symptom

`int32`/`int64`/`uint32` of a large double array containing exact half-integers
rounded ties to EVEN instead of MATLAB's half-away-from-zero — but only on the
SIMD path (≥ one vector of lanes); the scalar tail and the narrow types
(int8/int16) were already correct, which is why small-array probes missed it.

```matlab
x = repmat([0.5 1.5 2.5 3.5 4.5], 1, 64);   % 320 elems -> SIMD path
unique(int32(x))     % BUG: [0 2 4]   MATLAB: [1 2 3 4 5]
int32(-2.5)          % (large-array) BUG: -2   MATLAB: -3
```

`round()` and small/narrow casts (int8, int16) were unaffected (they use the
scalar `std::round` path).

## Root cause

`src/lang/src/types/casts_highway.cpp` — the wide-type SIMD bodies
`DoubleToInt32Loop` / `DoubleToUInt32Loop` / `DoubleToInt64Loop` used
`hn::Round(v)`, which is round-to-nearest-ties-to-even (RTNE). MATLAB's `int*()`
and `round()` use round-half-away-from-zero. The pre-existing comment had
documented the divergence as "bench-OK for now" (random inputs almost never hit
exact half-integers) and deferred the fix.

## Fix

Replaced `hn::Round(v)` with explicit half-away rounding
`hn::Trunc(hn::Add(v, hn::CopySign(hn::Set(d, 0.5), v)))` at all three wide-type
SIMD sites, matching the scalar `std::round` path. Narrow types (int8/16) and
uint64 already used the scalar path and were untouched.

Live guard: `IntegerTypesTest.IntCastHalfAwayOnSimdPath` in
`tests/lang/integer_types_test.cpp` (320-element array to force the SIMD
path; int32/int64/uint32, positive + negative, plus non-tie regression).
Verified on desktop-fast.
