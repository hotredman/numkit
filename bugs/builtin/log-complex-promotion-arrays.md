# builtin.log / log10 / log2 — real ARRAYS with a negative element don't promote to complex

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
- Note: `log1p` has a related gap (it never promotes — even a scalar `log1p(-2)`
  returns NaN; MATLAB `= log(-1) = pi*i`) and uses a different domain (`x<-1`,
  complex op `log(1+z)`). Not addressed here — a candidate follow-up.

## References
- `libs/builtin/src/math/exp_log/exp_log_{highway,portable}.cpp`
- MATLAB `doc log`, `doc log10`, `doc log2`
- Sibling: bugs/builtin/complex-promotion-arrays.md (sqrt/acosh/atanh, FIXED)
