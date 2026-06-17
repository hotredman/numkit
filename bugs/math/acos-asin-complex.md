# math.acos / asin — return NaN for |x|>1 instead of a complex value

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (NaN where MATLAB returns a complex result)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (complex-from-real sweep)

## Symptom
`acos`/`asin` of a real argument with `|x| > 1` return `NaN` (real-domain
only). MATLAB returns the complex value. The sibling inverse functions
`atanh`, `acosh`, `asinh` already go complex correctly, so `acos`/`asin` are
the outliers.

## Repro
```matlab
acos(2)
% numkit: NaN
% MATLAB: 0 + 1.3170i
asin(2)
% numkit: NaN
% MATLAB: 1.5708 - 1.3170i
atanh(2)   % numkit == MATLAB == 0.5493 + 1.5708i   (already complex)
acosh(0.5) % numkit == MATLAB == 0 + 1.0472i        (already complex)
```

## Root cause
The `acos`/`asin` kernels call the real `std::acos`/`std::asin` (→ NaN out of
`[-1,1]`) and never promote to the complex branch, unlike `sqrt`/`log`/`power`
/`acosh`/`atanh` which detect the out-of-domain input and produce a complex
result.

## Suggested fix
When a real input falls outside `[-1, 1]`, evaluate the complex inverse:
`acos(x) = -i·log(x + i·sqrt(1-x²))`, `asin(x) = -i·log(i·x + sqrt(1-x²))`
(or just call the existing complex `acos`/`asin` path on `complex(x,0)`).
Promote the whole array to complex if any element is out of range (MATLAB
semantics). Small — mirror how `sqrt`/`log` already branch.

## References
- `toolboxes/builtin/src/math/trig/trig_{highway,portable}.cpp` (acos/asin)
- MATLAB `doc acos`, `doc asin`

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 5).
- Added an out-of-`[-1,1]` check in both the Highway and portable acos/asin:
  if any real element is out of range the whole array is promoted to complex
  (MATLAB semantics). **Branch-cut gotcha:** `std::acos`/`std::asin` on the
  complex axis disagree with MATLAB's imaginary SIGN on `[1, +inf)`, so the
  result is computed via `acosh` to match exactly:
  `acos(x>1)=i·acosh(x)`, `acos(x<-1)=π−i·acosh(|x|)`,
  `asin(x>1)=π/2−i·acosh(x)`, `asin(x<-1)=−π/2+i·acosh(|x|)`. In-domain input
  and NaN stay real.
- Live guard: `toolboxes/builtin/tests/acos_asin_complex_test.cpp` (4 cases).
  Parity: `tools/parity/specs/{acos,asin}.json` extended (correctness=OK).
  Smoke: `toolboxes/builtin/tests/smoke/acos_asin_complex_smoke.m`.
- Spin-off finding: `sqrt`/`acosh`/`atanh` have the same *array* gap (promote
  only scalars) — catalogued as bugs/math/complex-promotion-arrays.md.
