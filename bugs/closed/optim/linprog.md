# optim.linprog — linear program

- **Status:** ✅ FIXED (2026-06-19) — proximal regularization over quadprog
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE (split from
  [constrained-solvers](constrained-solvers.md) 2026-06-19)

## Symptom
`linprog(f, A, b, …)` — minimise `fᵀx` subject to linear constraints — was
not registered.

## Repro
```matlab
linprog([1 1], [-1 0; 0 -1], [-1; -1])     % MATLAB: [1; 1]  (x1>=1, x2>=1)
% numkit: Error — VM: undefined function 'linprog'
```

## Fix (2026-06-19)
Implemented as an **embedded `.m`** (`linprog_reg.cpp`) that **reuses the
working quadprog active-set** via a **proximal (Tikhonov) regularization**:

```
min fᵀx + (ε/2)·‖x‖²      s.t.  A·x ≤ b, Aeq·x = beq, lb ≤ x ≤ ub   (ε = 1e-9)
```

Key fact that makes it **exact** (not an O(ε) approximation): at an LP
optimum that is a **vertex**, exactly `n` constraints are active and fully
determine `x` through a linear solve — *independent* of the `ε·I` term — so
the regularized QP returns the exact vertex. (Verified to be ε-invariant
across ε = 1e-6 … 1e-10.) The `ε` term only decides *which* active set is
optimal, and for small ε the `fᵀx` term dominates → the LP-optimal vertex.

Verified vs MATLAB R2025b on unique-optimum LPs (exact): lower-bound
`x1≥1,x2≥1 → [1 1]` (fval 2); classic `max 3x+2y s.t. x+y≤4, x+3y≤6, x≥0
→ [4 0]` (fval −12); `min −x1−2x2 s.t. x1+x2≤4, x1≤3, x≥0 → [0 4]`; box
bounds `[0,2]×[0,3] → [2 3]`. Returns a column `x`. Parity `linprog.json`
→ OK.

## Caveats / scope (follow-up: a true 2-phase simplex)
- **Degenerate optimum** (an optimal *face*, not a single vertex): the
  regularization returns the minimum-`‖x‖` optimal point, which can differ
  from MATLAB's *basic vertex* (the **objective value still matches**). E.g.
  `min x1+x2 s.t. x1+x2=5, 0≤x≤4` — MATLAB returns the vertex `[1 4]`, this
  returns the midpoint `[2.5 2.5]`. A proper simplex would match the vertex.
- **Unboundedness** is only flagged heuristically (`‖x‖` blows up →
  `exitflag = −3`); **infeasibility** is not robustly detected. A 2-phase
  simplex (or interior-point) is the exact follow-up for these.

## References
- `src/bundle/src/register/optim/linprog_reg.cpp` (`kLinprogMSource`),
  `optim_library.cpp` (registration)
- reused: the `quadprog` active-set
- `tools/parity/specs/linprog.json`,
  `src/toolboxes/optim/tests/linprog_test.cpp` (4 cases),
  `known_bugs_test.cpp` (`Linprog`, promoted live),
  smoke `tests/smoke/linprog_smoke.m`
- MATLAB `doc linprog`
