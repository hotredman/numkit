# math.numeric — real-code numeric divergence ~1e-7 rel in arithmetic-coding accumulation (chap10_7)

- **Status:** 🔴 OPEN (diagnosis pending)
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

- **Guard:** deferred — minimal standalone repro pending diagnosis (the
  corpus script is large; the fieldtest batch is the reproducer).
- Fieldtest report: `fieldtest/reports/20260830-135536.json` (output-mismatch).
- Namespace may move (math→stats/builtin) once the diverging builtin is
  identified.
