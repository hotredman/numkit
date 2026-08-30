# lang.* — sa_01knapsack: selection vector and energy match MATLAB, one output token diverges

- **Status:** 🔴 OPEN (triage pending — small)
- **Severity:** P2 (one diverging value in otherwise-matching output)
- **Kind:** bug
- **Found:** 2026-08-30 via the fieldtest corpus (Algorithms_MathModels sa_01knapsack.m — simulated annealing knapsack)

## Symptom

The knapsack selection vector `[1 1 0 0 1 1 1 1 1 1 0 0]` and the energy
(76) match MATLAB exactly; ONE further token diverges (46 vs something
else) — likely a tie-breaking difference in the annealing acceptance or a
print-format edge, not the optimization itself.

## Repro

```matlab
clear;
% fieldtest corpus: fieldtest/corpus/work/Algorithms_MathModels/…/sa_01knapsack.m
% Run via the fieldtest harness: selection + energy match; one token differs.
```
(Standalone minimal repro = the triage step; suspect the random-stream or
a rounding comparison in the acceptance rule.)

## Root cause (hypotheses)

1. `rand`-stream tie-break at an acceptance boundary (annealing accepts on
   an exact comparison MATLAB rounds the other way);
2. A `fprintf` format nuance printing the differing token.

## Suggested fix

Instrument the script against MATLAB token-by-token (the harness diff
already localizes the line); then either fix the diverging builtin or
close as documented tie-break noise.

## References

- **Guard:** deferred — minimal repro pending triage (corpus script is the
  reproducer; fieldtest report `20260830-135536.json`).
- Fieldtest output-mismatch class.
