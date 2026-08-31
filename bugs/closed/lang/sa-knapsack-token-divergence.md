# lang.* — sa_01knapsack: "one output token diverges" — triaged NOT a defect (stochastic trajectory, identical optimum)

- **Status:** ✅ CLOSED — not a defect (2026-08-31, triaged via the R4 .mat comparison)
- **Severity:** —
- **Kind:** bug (filed in error; closed after triage)
- **Found:** 2026-08-30 via fieldtest (real-world corpus, sa_01knapsack.m)

## Symptom (as filed)

The knapsack selection vector and the energy (76) matched MATLAB in the
stdout fuzzy-diff; ONE further printed token diverged between engines.

## Triage (R4 workspace comparison, 2026-08-31)

Dual-run with `save()` of both workspaces and a variable-level diff:

- **Final-state variables are IDENTICAL**: `sol_best = [1 1 0 0 1 1 1 1 1 1 0 0]`,
  `E_best = -76`, weight `46` — both engines print exactly these values.
- Diverged variables are the TRANSITIONAL annealing state only
  (`E_current`, `E_new`, `sol_current`, `sol_new`, `tmp`).
- The `rand` streams start IDENTICAL (first draws `0.814724, 0.905792,
  0.126987` — the seed-0 Mersenne Twister head, both engines).

## Root cause

Chaotic amplification, not an engine defect: simulated annealing accepts
or rejects on `rand < exp(-(ΔE)/t)`; a 1-ulp difference in `exp` at any
decision boundary flips one acceptance, which changes the number of
subsequent loop iterations and therefore the draw INDEX alignment for the
rest of the run. Trajectories diverge; the algorithm still converges to
the same optimum. Stochastic search code can never be token-compared
across engines — only its final state can.

## Repro of the equivalence (copy-paste)

```matlab
clear;
% Same instance, same seed head — both engines:
disp(rand(3,1))            % 0.8147 0.9058 0.1270 in numkit AND MATLAB R2025b
% ...run any simulated-annealing loop; final sol_best/E_best match,
% intermediate E_current/sol_current diverge after the first 1-ulp
% acceptance flip (expected for chaotic stochastic dynamics).
```

## Resolution

No fix warranted. The verdict-classification lesson is absorbed by R4
(workspace .mat comparison): stdout token diffs on stochastic scripts are
inherently uninterpretable; `workspace-mismatch` on FINAL variables is
the comparable signal.
