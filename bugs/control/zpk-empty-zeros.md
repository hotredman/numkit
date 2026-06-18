# control.zpk / zp2tf — empty zeros drop the gain k

- **Status:** ✅ FIXED (2026-06-19) — zp2tf empty-zero → num=[k]
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

## Fix (2026-06-19)
In `zp2tf` (`conversion.cpp`), normalize the empty-`z` case: when
`math::poly(z)` returns an empty row (numel 0), replace `num` with `[1]`
before the gain multiply, so `num = k·[1] = [k]` (then zero-padded to the
denominator length downstream by `tfdata`). The deeper cause is
`math::poly([])` returning an empty row instead of `[1]` (MATLAB's
`poly([])==1`); the localized control-side guard fixes the reported
symptom without touching the shared math layer. The non-empty-zero path is
unchanged.

Verified vs MATLAB R2025b (parity `zpk_empty_zeros.json` → OK):
`zpk([],[-1 -2],2)` → `num=[0 0 2]` (was `[0 0 0]`); `zpk([],[-1 -2 -3],1)`
→ `[0 0 0 1]` (was `[0 0 0 0]`); `zpk(-5,[-1 -2 -3],1)` → `[0 0 1 5]`
(unchanged). Guard: `known_bugs_test.cpp` (`ZpkEmptyZerosGain`, promoted live).

## References
- `src/toolboxes/control/src/conversion/conversion.cpp` (`zp2tf`)
- `tools/parity/specs/zpk_empty_zeros.json`
- guard: `known_bugs_test.cpp` (`ZpkEmptyZerosGain`)
- found via bugs/control/allmargin.md (zpk input path)
- deeper cause (not fixed here): `numkit::math::poly([])` returns `[]` not `[1]`
- MATLAB `doc zpk`, `doc zp2tf`
