# builtin.diff — silently drops the imaginary part on complex input

- **Status:** 🔴 OPEN
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
`diff` (`libs/builtin/src/language/arrays/matrix.cpp`) computes successive
differences over `doubleData()` only — the complex elements' imaginary parts
are never differenced (the result is materialised real, or complex with a
zero imaginary part).

## Suggested fix
Add a `ValueType::COMPLEX` branch: difference `complexData()` element-wise
(real and imaginary independently), honouring `n` (order) and `dim`. Output a
`Value::complexMatrix`. Small — mirror the real path on `Complex`.

## References
- `libs/builtin/src/language/arrays/matrix.cpp` (diff)
- MATLAB `doc diff`
- Related: cumsum/cumprod also lack complex (bugs/builtin/cumsum-complex.md);
  a broader survey is bugs/builtin/complex-input-unsupported.md.
