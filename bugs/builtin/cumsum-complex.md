# builtin.cumsum / cumprod — complex input throws

- **Status:** 🔴 OPEN
- **Severity:** P2 (errors where MATLAB returns a value)
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (complex-input sweep)

## Symptom
`cumsum` and `cumprod` throw "Not a double array" on a complex input. MATLAB
accumulates complex values element-wise. (`sum`, `prod`, `mean`, `cumtrapz`
all handle complex — `cumsum`/`cumprod` are the outliers.)

## Repro
```matlab
cumsum([1+1i 2+2i])
% numkit: Error — Not a double array
% MATLAB: [1+1i  3+3i]
cumprod([1+1i 1-1i])
% numkit: Error — Not a double array
% MATLAB: [1+1i  2+0i]
```

## Root cause
`cumsum`/`cumprod` (`libs/builtin/src/language/arrays/matrix.cpp:2026`, regs
~3716/3724; SIMD kernels in `math/arithmetic/cumsum_*.cpp`) read
`x.doubleData()` directly — there is no `ValueType::COMPLEX` branch, so a
complex Value trips the "Not a double array" guard.

## Suggested fix
Add a complex path: scan `x.complexData()` and accumulate `Complex` running
sums/products into a `Value::complexMatrix` of the same shape, honouring the
`dim` and `'reverse'` flags. Mirror the existing real kernel's dim handling.
Small–moderate (one complex branch per function). `sum`/`prod` already have
the complex accumulation logic to copy.

## References
- `libs/builtin/src/language/arrays/matrix.cpp` (cumsum/cumprod + regs)
- `libs/builtin/src/math/arithmetic/cumsum_{highway,portable}.cpp`
- MATLAB `doc cumsum`, `doc cumprod`
