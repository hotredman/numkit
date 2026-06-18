# control.zpk / zp2tf — empty zeros drop the gain k

- **Status:** 🔴 OPEN
- **Severity:** P2 (wrong numeric result)
- **Kind:** bug
- **Found:** 2026-06-19 while wiring `allmargin` (zpk input path)

## Symptom
A `zpk` model with **no finite zeros** loses its gain `k` when converted to
a transfer function: the numerator comes out all-zero instead of `[k]`. A
`zpk` **with** at least one finite zero converts correctly, so the defect is
specific to the empty-zero (`z = []`) branch of `zp2tf`.

## Repro
```matlab
[n, d] = tfdata(zpk([], [-1 -2], 2), 'v');
% MATLAB: n = [0 0 2],  d = [1 3 2]      (2 / ((s+1)(s+2)))
% numkit: n = [0 0 0],  d = [1 3 2]      (gain 2 dropped → zero system)

[n, d] = tfdata(zpk([], [-1 -2 -3], 1), 'v');
% MATLAB: n = [0 0 0 1];  numkit: n = [0 0 0 0]

% Control: a finite zero converts fine —
[n, d] = tfdata(zpk(-5, [-1 -2 -3], 1), 'v');   % numkit n = [0 0 1 5]  (correct)
```

## Root cause (hypothesis)
`zp2tf(z, p, k)` builds the numerator as `k · poly(z)`. With `z = []`,
`poly([])` should be `[1]` (the empty product), giving `num = [k]`; instead
the empty-zero branch yields an all-zero numerator (the `k` scaling is lost
or `poly([])` returns an empty/zero vector). The denominator `poly(p)` is
correct, so only the numerator path for empty `z` is affected.

## Impact
Any `zpk` built as a pure pole model (`zpk([], poles, k)`) — common for
plain low-pass / all-pole plants — becomes the zero system after any
tf-domain operation (`tfdata`, `tf`, `bode`, `step`, `allmargin`, …).
`allmargin` reaches it via its zpk→`zp2tf` path; the tf input path is
unaffected.

## Suggested fix
In `zp2tf` (toolboxes/control `conversion.cpp`), make the empty-`z` case
return `num = [k]` (then zero-pad to the denominator length downstream).
Equivalently, ensure `poly([])` yields `[1]` and the `k` multiply runs
regardless of the zero count. Verify `zpk([],[-1 -2],2)` → `[0 0 2]` and a
mixed case against MATLAB.

## References
- `src/toolboxes/control/src/conversion/conversion.cpp` (`zp2tf`)
- guard: `known_bugs_test.cpp` (`DISABLED_ZpkEmptyZerosGain`)
- found via bugs/control/allmargin.md (zpk input path)
- MATLAB `doc zpk`, `doc zp2tf`
