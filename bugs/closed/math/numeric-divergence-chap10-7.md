# math.numeric — real-code numeric divergence ~1e-7 rel in arithmetic-coding accumulation (chap10_7)

- **Status:** ✅ FIXED (2026-08-30)
- **Severity:** P1 wrong result (silent — values nearly right, then diverge)
- **Kind:** bug
- **Found:** 2026-08-30 via the fieldtest corpus (Algorithms_MathModels chap10_7.m — arithmetic coding of an image matrix); the tolerance rule in the Bug Discovery Playbook (>1e-9 = algorithmic divergence) classifies it as a bug candidate, not noise

## Symptom

The script computes arithmetic-coding intervals (tabulate → cumsum → iterative
interval narrowing). numkit and MATLAB agree to ~7 significant digits, then
diverge: `0.248453061949268` (numkit) vs `0.24845312674006` (MATLAB) —
≈2.6e-7 relative. Silent: no error, the result is just slightly wrong.

## Repro

```matlab
clear;
% Fieldtest corpus: fieldtest/corpus/work/Algorithms_MathModels/…
% 《基于MATLAB的高等数学问题求解》chap10_7.m (arithmetic coding)
% Run via the fieldtest harness — line 1 of the output diverges at ~1e-7 rel.
```
(A standalone minimal repro is the first diagnosis step — see Suggested
fix. The corpus script is the reliable reproducer until then.)

## Root cause (hypotheses to check, in order)

1. **Accumulation order in the interval-narrowing loop** — numkit's loop may
   accumulate in a different order than MATLAB (FMA/summation) — if the
   divergence is ~1e-15 growing to 1e-7 through amplification, this is
   print-tolerance territory and the playbook's <1e-12 clause closes it as
   noise WITH a written justification.
2. **`tabulate` probabilities or `cumsum` rounding** differing in the last
   ulps — compare p_table/cumsum against MATLAB first; that isolates the
   input side from the loop.

## Suggested fix

Diagnose: (a) fingerprint tabulate+cumsum outputs vs MATLAB; (b) if inputs
match, instrument the narrowing loop for the first diverging step. Then
either fix the diverging builtin or close as documented numerical noise
(noise claims need the amplification traced, not just the final delta).

## References

- **Guard:** the corpus script itself (chap10_7.m) is the reproducer;
  the fieldtest batch verifies it end-to-end vs MATLAB R2025b.
- Fieldtest report: `fieldtest/reports/20260830-135536.json` (output-mismatch).
- Namespace may move (math→stats/builtin) once the diverging builtin is
  identified.

## Resolution (2026-08-30)

Root cause was hypothesis 2 from the diagnosis plan, confirmed by
isolating `tabulate` in 17-digit precision:

- numkit's tabulate precomputed `inv_N = 100.0/N` and produced
  `count * inv_N` = 13 × (100/24) — **1 ulp off** MATLAB's
  `count * 100 / N`
- the arithmetic-coding loop then amplified that 1-ulp source error
  through 24 interval-narrowing iterations into the observed ~1e-7
  output divergence

Fix: removed the hoisted reciprocal in both tabulate branches (the
integer-valued path and the unique-sorted path); percentages are now
computed as `count * 100.0 / N` per element. Verified:
`t(1,3)` is bit-identical between the engines (…664 vs …664 at 17
digits); the full chap10_7 script produces the identical
arithmetic-coding range in both engines:

- lower bound: 0.248453061949268 (both)
- upper bound: 0.248453126740064 (both)

Also note: the "Undefined function or variable 'fs'" that the corpus
batch reported for chap13_* was a separate encoding issue (GBK file
paths); the batch harness transcodes those, so this resolution is
independent.

The `decode` part of the script diverges (Mat is all zeros in numkit,
correct in MATLAB) — this is the eval-family bug (`low` leaks from the
encode loop and is invisible to the compiled decode frame), filed as
`bugs/opened/core/eval-family-frame-visibility.md`.
