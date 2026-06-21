# optim.quadprog — quadratic program

- **Status:** ✅ FIXED (2026-06-19) — embedded-.m primal active-set (strictly-convex)
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE (split from
  [constrained-solvers](constrained-solvers.md) 2026-06-19)

## Symptom
`quadprog(H, f, …)` — minimise `0.5·xᵀHx + fᵀx` subject to linear
constraints — was not registered.

## Repro
```matlab
quadprog(eye(2), [-1 -1])                    % MATLAB: [1; 1]
quadprog(eye(2), [-1 -1], [1 1], 1)          % MATLAB: [0.5; 0.5]  (x1+x2<=1)
% numkit (each): Error — VM: undefined function 'quadprog'
```

## Fix (2026-06-19)
Implemented as an **embedded `.m`** (`quadprog_reg.cpp`), same self-contained
footing as the other optim solvers. **Primal active-set** for a
strictly-convex QP (`H` positive definite), which needs **no Phase-1**: it
starts from the unconstrained minimum `−H\f` and each iteration solves the
equality-constrained QP over the current working set via the KKT
saddle-point system

```
[H Bᵀ; B 0]·[x; λ] = [−f; c]      B = [Aeq; active inequalities],  c = [beq; …]
```

then **adds the most-violated** inactive inequality or **drops the
most-negative**-multiplier active one until the KKT conditions hold. Bounds
`lb ≤ x ≤ ub` are folded into the inequality set (`x ≤ ub`, `−x ≤ −lb`). The
QP optimum is **unique** for PD `H`, so the active-set path is irrelevant to
the result — it matches MATLAB quadprog's solution exactly. Returns a column
`x`; split into `quadprog` + `nk_qp_activeset` + `nk_qp_kkt`.

Verified vs MATLAB R2025b across every constraint type: unconstrained
`→ [1 1]`; inequality `x1+x2≤1 → [0.5 0.5]` (fval −0.75); equality
`x1+x2=3 → [1.5 1.5]`; bounds `[0,0.3] → [0.3 0.3]`; non-identity
`H=diag(2,4), f=[−2;−8], x1+x2≤1 → [−1/3 4/3]`; two active inequalities
`→ [0.3 0.7]`. Parity `quadprog.json` → OK.

Scope: strictly-convex `H` (the common case). A non-PD / indefinite `H`
(non-convex QP) is not handled specially; the `x0` and `options` args are
accepted but unused.

## References
- `src/bundle/src/register/optim/quadprog_reg.cpp` (`kQuadprogMSource`),
  `optim_library.cpp` (registration)
- `tools/parity/specs/quadprog.json`,
  `src/toolboxes/optim/tests/quadprog_test.cpp` (6 cases),
  `known_bugs_test.cpp` (`Quadprog`, promoted live),
  smoke `tests/smoke/quadprog_smoke.m`
- MATLAB `doc quadprog`
