# optim.linprog — bounded LP returns an unbounded, bound-violating point (objVal −3.3e9 vs MATLAB −1.2)

- **Status:** ✅ FIXED (043c8b4b2, 2026-09-06)
- **Kind:** bug
- **Severity:** P1 wrong result (silently incorrect, constraint-violating "solution")
- **Found:** 2026-09-06 via the springer-math compare group — example1a/example1b (linear-programming-using-MATLAB, appendix A) flagged objVal/x divergence after their rank-deficiency blocker was closed

## Symptom

The KKT-regularized solve (portion 11's −1e-12·I fix) diverges on this
bounded LP: numkit returns x with x3 ≈ −7.1e8 (lower bound is 2!) and
objVal ≈ −3.3e9; MATLAB finds the true optimum.

## Repro

```matlab
clear;
c = [-2; 4; -2; 2];
A = [-2 0 -3 0; -3 2 0 -4]; b = [-6; -8];
Aeq = [4 -3 8 -1; 4 0 -1 4]; beq = [20; 18];
lb = [1 0 2 0]; ub = [Inf Inf 10 Inf];
[x, objVal] = linprog(c, A, b, Aeq, beq, lb, ub);
% MATLAB R2025b:  objVal = -1.2,  x = [1.8 0 2 3.2]
% numkit:         objVal = -3.3e9, x = [1.1e9 0 -7.1e8 -1.3e9]  (violates lb!)
```

## Root cause

The constraint-accumulation active set CYCLED: with H = eps*I (the
linprog regularization) the delta-regularized saddle returns
multipliers at O(1/delta) = 1e12 scale, whose SIGNS are noise — the
drop step removes wrong constraints, the loop re-adds them, hits the
iteration cap (60+5m) and then RETURNS THE LAST GARBAGE x silently
(traced live: the active set oscillated over all 110 iterations,
intermediate x already at 1e9).

## Fix

nk_qp_activeset is REPLACED by nk_qp_admm: an OSQP-style ADMM (no
feasible start, rank-deficiency tolerant) plus an exact POLISH that
solves the equality-KKT on the final active set through the existing
nk_qp_kkt saddle and keeps the vertex when feasible and at least as
good. Live guard: `OptimKnownBug.LinprogBoundedLpOptimum` (this
repro, exact values). All 161 optim-suite tests green, including the
portion-11 rank-deficient repro and fmincon's SQP (a quadprog
consumer).

## References

- Guard: `OptimKnownBug.LinprogBoundedLpOptimum` (live) in
  `src/toolboxes/optim/tests/known_bugs_test.cpp`
