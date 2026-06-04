# builtin.sqrt / acosh / atanh — real ARRAYS don't promote to complex (scalar does)

- **Status:** 🔴 OPEN
- **Severity:** P2 (silently wrong — NaN where MATLAB returns a complex value)
- **Kind:** bug
- **Found:** 2026-06-05 while fixing acos/asin complex (bug-fix loop, cycle 5)

## Symptom
`sqrt`, `acosh`, `atanh` (and the other real→complex inverse functions)
promote a real input to complex **only when it is a scalar**. For an ARRAY
with out-of-domain elements they fall through to the real libm path and emit
`NaN` for those elements, where MATLAB promotes the whole array to complex.

## Repro
```matlab
sqrt([-1 4])
% numkit: [NaN 2]        (real)
% MATLAB: [0+1i 2]       (complex)
acosh([0.5 2])
% numkit: [NaN 1.31696]  (real)
% MATLAB: [0+1.0472i 1.31696]
atanh([2 0.5])
% numkit: [NaN 0.5493]   (real)
% MATLAB: [0.5493-1.5708i 0.5493]   (complex)
```

## Root cause
The kernels (`libs/builtin/src/math/exp_log/exp_log_{highway,portable}.cpp`
for sqrt; `libs/builtin/src/math/trig/trig_{highway,portable}.cpp` for
acosh/atanh) guard the out-of-domain branch with `if (x.isScalar() && …)`.
There is no array path: a vector with any out-of-range element skips the
complex branch entirely.

## Suggested fix
Mirror what acos/asin got in the 2026-06-05 fix: a small `anyOutside…`
predicate over the elements, and when true promote the whole array
(`Value cx = x; cx.promoteToComplex(mr);`) before applying the complex op
(or an element-wise real→complex map). Apply uniformly to sqrt / acosh /
atanh (asinh is entire — no domain limit). Small. Validate each vs MATLAB on
a mixed in/out-of-domain vector.

## References
- `libs/builtin/src/math/exp_log/exp_log_{highway,portable}.cpp` (sqrt)
- `libs/builtin/src/math/trig/trig_{highway,portable}.cpp` (acosh/atanh)
- Pattern to copy: the acos/asin array promotion in the same trig files
  (bugs/builtin/acos-asin-complex.md, FIXED).
- MATLAB `doc sqrt`, `doc acosh`, `doc atanh`
