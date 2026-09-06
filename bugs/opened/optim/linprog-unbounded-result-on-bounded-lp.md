# optim.linprog — bounded LP returns an unbounded, bound-violating point (objVal −3.3e9 vs MATLAB −1.2)

- **Status:** 🔴 OPEN
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

Not yet diagnosed — the regularized KKT solve is returning a point far
outside the box; the active-set/bound handling appears to accept the
unconstrained KKT step. The portion-11 repro (rank-deficient proximal
LP) stays green, so this is a different failure branch.

## Suggested fix

Diagnose the bound-constraint path in quadprog's KKT solve (which
linprog feeds): likely missing rejection/clamping of steps that leave
the box, or the regularizer interacting with the equality-constrained
subproblem. Cross-check against OSQP-style primal-dual updates; add
this repro as a live guard once fixed.

## References

- Guard: `DISABLED_LinprogBoundedLpOptimum` in
  `src/toolboxes/optim/tests/known_bugs_test.cpp`
- Affected corpus scripts: example1a.m, example1b.m
  (`# known: optim/linprog-unbounded-result-on-bounded-lp` in the group)
