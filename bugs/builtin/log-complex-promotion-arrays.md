# builtin.log / log10 / log2 / log1p — real ARRAYS out of domain don't promote to complex

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (silently wrong — NaN where MATLAB returns a complex value)
- **Kind:** bug
- **Found:** 2026-06-05 via the complex-promotion sweep (sibling of
  bugs/builtin/complex-promotion-arrays.md, which covered sqrt/acosh/atanh)

## Symptom
`log`, `log10`, `log2` promote a negative input to complex only when it is a
SCALAR. For an ARRAY with any negative element they fell through to the real
libm path and emitted `NaN` for those elements, where MATLAB promotes the
whole array to complex.

## Repro
```matlab
log([-1 1])
% numkit: [NaN 0]         (real)
% MATLAB: [0+pi*i 0]      (complex)
log10([-100 100])
% numkit: [NaN 2]
% MATLAB: [2+1.36438i 2]
log2([-8 8])
% numkit: [NaN 3]
% MATLAB: [3+4.53236i 3]
log(-1)   % scalar already worked: 0+pi*i
```

## Root cause
The kernels (`libs/builtin/src/math/exp_log/exp_log_{highway,portable}.cpp`)
guarded the complex branch with `if (x.isScalar() && x.toScalar() < 0)`; a
vector with a negative element skipped it. Identical to the sqrt/acosh/atanh
array gap (bugs/builtin/complex-promotion-arrays.md).

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 16).
- Added an `anyNegative` check (the same predicate the sqrt fix introduced,
  forward-declared for the log functions) and, when it fires, promote the whole
  array (`Value cx = x; cx.promoteToComplex(mr);`) and apply the complex op:
  `std::log` for `log`, `std::log(c)/log(2)` for `log2`, `std::log10` for
  `log10` — these branches all match MATLAB. In-domain input and the scalar
  paths are unchanged. Highway + portable.
- Live guard: `libs/builtin/tests/log_complex_test.cpp`. Parity:
  `tools/parity/specs/{log,log10,log2}.json` extended (correctness=OK). Smoke:
  `libs/builtin/tests/smoke/log_complex_smoke.m`.
## log1p follow-up (FIXED 2026-06-05, cycle 19)
`log1p` had the same gap on a *different* domain: `log1p(x) = log(1+x)` is
complex for `x < -1` (e.g. `log1p(-2) = log(-1) = i·π`), but log1p never
promoted — even a scalar `log1p(-2)` returned NaN.
- Fixed in `exp_log_{highway,portable}.cpp`: scalar `x < -1` → complex; any
  array element `< -1` promotes the whole real array to complex; complex input
  uses `log(1+z)`.
- The promoted array is filled **accurately per element** — `log1p` is kept for
  `x >= -1` (so `log1p([-2, 1e-15])(2)` stays `1e-15`, not the lossy
  `log(1+1e-15) = 1.11e-15`); for `x < -1` it is `log|1+x| + i·π`. Verified vs
  MATLAB R2025b: `log1p(-2)=i·π`, `log1p(-3)=log(2)+i·π`,
  `log1p([-2 -0.5 0 3])=[i·π, -0.69315, 0, 1.38629]`, `log1p(3+4i)=1.732868+
  0.785398i`, `log1p(-1)=-Inf`, in-domain stays real.
- Guards: `libs/builtin/tests/log1p_complex_test.cpp` (5 TEST_F), parity
  `tools/parity/specs/log1p.json` (extended; correctness=OK), smoke
  `libs/builtin/tests/smoke/log1p_complex_smoke.m`. This closes the whole
  promotion family (sqrt/acosh/atanh/acos/asin/log/log10/log2/log1p).

## References
- `libs/builtin/src/math/exp_log/exp_log_{highway,portable}.cpp`
- MATLAB `doc log`, `doc log10`, `doc log2`, `doc log1p`
- Sibling: bugs/builtin/complex-promotion-arrays.md (sqrt/acosh/atanh, FIXED)
