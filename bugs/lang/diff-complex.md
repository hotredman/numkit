# lang.diff — silently drops the imaginary part on complex input

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P1 (silently wrong result)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (complex-input sweep)

## Symptom
`diff` on a complex array keeps only the REAL part of each difference and
zeroes the imaginary part — it returns a (wrong) value rather than erroring,
so the bug is silent.

## Repro
```matlab
diff([1+2i 4+6i 9+12i])
% numkit: [3+0i  5+0i]
% MATLAB: [3+4i  5+6i]
diff([1+1i 3+2i])
% numkit: 2+0i
% MATLAB: 2+1i
```

## Root cause
`diff` (`src/lang/src/arrays/matrix.cpp`) computes successive
differences over `doubleData()` only — the complex elements' imaginary parts
are never differenced (the result is materialised real, or complex with a
zero imaginary part).

## Suggested fix
Add a `ValueType::COMPLEX` branch: difference `complexData()` element-wise
(real and imaginary independently), honouring `n` (order) and `dim`. Output a
`Value::complexMatrix`. Small — mirror the real path on `Complex`.

## References
- `src/lang/src/arrays/matrix.cpp` (diff)
- MATLAB `doc diff`
- Related: cumsum/cumprod also lack complex (bugs/lang/cumsum-complex.md);
  a broader survey is bugs/math/complex-input-unsupported.md.

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 3).
- Added `diffOnceComplex` (mirrors `diffOnceDouble` over `Complex` storage),
  generalised `makeDiffOutput` with a `ValueType` parameter, and added a
  `ValueType::COMPLEX` branch in `diff()` (n-th order + dim, via
  `copyComplexSameShape` + the complex pass loop). The `n==0` identity path
  now also preserves both parts.
- Live guard: `tests/builtin/diff_complex_test.cpp` (4 cases). Parity:
  `tools/parity/specs/diff.json` extended (correctness=OK). Smoke:
  `tests/builtin/smoke/diff_complex_smoke.m`.
- Spin-off finding: numkit accepts `diff(X,0)` (returns identity) where MATLAB
  errors — catalogued separately as bugs/lang/diff-zero-order.md.
