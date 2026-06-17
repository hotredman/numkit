# math.max / min — elementwise on complex errors "Not a double array"

- **Status:** ✅ FIXED (2026-06-17)
- **Severity:** P2 (missing feature; numkit errored, no silent-wrong result)
- **Kind:** stub
- **Found:** 2026-06-17 via complex-input fusion work (probe of per-op complex)

## Symptom
Elementwise `max(A,B)` / `min(A,B)` (and hence `clamp`-style `max(lo,min(hi,z))`)
threw `Not a double array` when an operand was complex. MATLAB supports them:
compare by magnitude `|z|`, ties broken by phase `angle(z)` (max → larger angle,
min → smaller), and a NaN-component operand is omitted (the other wins).

The single-arg REDUCTION `max(z)` / `min(z)` already worked (by magnitude).

## Repro
```matlab
max([3+4i, 0.2-0.1i], 1)   % MATLAB: [3+4i, 1]      ; numkit (was): ERROR "Not a double array"
min([3+4i, 0.2-0.1i], 1)   % MATLAB: [1, 0.2-0.1i]  ; numkit (was): ERROR
max(1+0i, 0+1i)            % MATLAB: 0+1i (tie |·|=1 → larger angle) ; numkit (was): ERROR
max(1+1i, 1-1i)            % MATLAB: 1+1i            ; numkit (was): ERROR
max(complex(NaN,2), 3+4i)  % MATLAB: 3+4i (NaN omitted) ; numkit (was): ERROR
max(complex(-3,0), 1)      % MATLAB: -3+0i (|−3|=3>1; NO all-real fallback in binary form)
```

## Root cause
`numkit::math::max/min(const Value&, const Value&, mr)` and the omit-NaN variants
`maxOmitNanBinary`/`minOmitNanBinary` (`src/math/src/arithmetic/reductions.cpp`)
went straight to `dispatchIntegerBinaryOp` then `elementwiseDouble(...fmax/fmin)`,
which calls `doubleData()` on a complex operand → throws. No complex branch.

The earlier note here called this a "VM-dispatch blocker" — that was a
**misdiagnosis**. `max`/`min` default to **`omitnan`** (`stripTrailingNanFlag`
sets it true), so `max_reg`'s 2-arg path calls **`maxOmitNanBinary`**, NOT
`max(Value,Value,mr)`. The previous diagnostic `throw` was placed in
`max(...)` (the `includenan` overload), which the default path never reaches —
hence it "didn't fire" while `max_reg` did. Nothing wrong with `execCallBuiltin`;
it routes complex correctly to the registered builtin (the 2-arg scalar fast
path is skipped because a complex array isn't `isDoubleScalar()`).

## Fix
Added a complex branch at the top of all four binary functions (`max`, `min`,
`maxOmitNanBinary`, `minOmitNanBinary`): when either operand is complex, route
through `elementwiseComplex` (broadcast + real→complex promotion) with a new
per-element picker `complexMinMaxPick<IsMax>(a, b, omitNan)` in
`reductions_detail.hpp`, next to the existing `complexBetter<IsMax>` comparator.

MATLAB R2025b semantics (validated by probe + `tools/parity/specs/maxmin_complex.json`):
compare by `|z|`, ties by `angle(z)`; **no all-real fallback** in the binary form
(`max(complex(-3,0),1)` = -3, unlike the reduction which falls back to real
parts). NaN: a value with EITHER component NaN is "missing" — omitnan (default)
skips it (both missing → first operand); includenan propagates it. `clamp`
(`max(lo,min(hi,z))`) works for free, which unblocks the complex clamp fusion
rule. Guard: `tests/builtin/maxmin_complex_test.cpp` (7 TEST_P × both backends).

## Resolved boundary
`max([1 -3 2], 2+0i)` now returns `[2 2 2]` (matches MATLAB): `2+0i` narrows to a
real double at the arithmetic source (2026-06-17), so max compares by value, not
`|z|`. `complex(x,0)` (forced complex) still compares by `|z|` in both engines.
A niche residual (forced-complex fed through pure structural ops, which don't yet
narrow) is tracked in [complex-zero-imag-narrowing](complex-zero-imag-narrowing.md).

## References
- `src/math/src/arithmetic/reductions.cpp` (max/min + omitnan binary, `complexBinaryMinMax`)
- `src/math/src/arithmetic/reductions_detail.hpp` (`complexMinMaxPick` / `complexBetter`)
- `tests/builtin/maxmin_complex_test.cpp`, `tools/parity/specs/maxmin_complex.json`
- Related (closed in the same effort): complex floor/ceil/round/fix + expm1
  (commit b2f30c53, `tests/builtin/.../complex_math_test.cpp`).
