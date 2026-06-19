# optim.fmincon — constrained minimization

- **Status:** ✅ FIXED (2026-06-19) — embedded-.m SQP over quadprog
  (linear + bound + nonlinear constraints)
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE (split from
  [constrained-solvers](constrained-solvers.md) 2026-06-19)

## Symptom
`fmincon(fun, x0, …)` — constrained nonlinear minimization — was not
registered.

## Repro
```matlab
fmincon(@(x) x(1)^2+x(2)^2, [1 1], [],[],[],[], [0 0],[2 2])  % MATLAB: [0; 0]
% numkit: Error — VM: undefined function 'fmincon'
```

## Fix (2026-06-19)
Implemented as an **embedded `.m`** (`fmincon_reg.cpp`) — a **Sequential
Quadratic Programming (SQP)** method that **reuses the shipped `quadprog`**
as the QP subproblem solver. At each iterate x: FD gradient `g = ∇f`, a BFGS
Hessian estimate `B` of the objective, and the step `d` from

```
min 0.5 dᵀB d + gᵀd   s.t.  A·d ≤ b−Ax,  Aeq·d = beq−Aeq x,  lb−x ≤ d ≤ ub−x
```

followed by a backtracking line search on `f`. The QP keeps the linear/bound
constraints feasible and the feasible region is convex, so every `x + α·d`
(α∈[0,1]) stays feasible — no merit function needed. BFGS update; iterate to
a vanishing step. Split into `fmincon` + `nk_fc_obj` + `nk_fc_grad` +
`nk_fc_sqp`.

Parity with MATLAB R2025b is on the **solution**: bounds-only
`min x1²+x2² in [0,2]² → [0 0]`; linear inequality `x1+x2≤2` with a far
objective center `→ [1 1]`; equality `x1+x2=2 → [1 1]`; objective minimum
outside the box clamps to the corner `→ [2 0]`. Output mirrors x0's
orientation. Parity `fmincon.json` → OK.

## Nonlinear constraints (`nonlcon`) — supported (2026-06-19)
`nonlcon` returns `[c, ceq]` (`c ≤ 0`, `ceq = 0`). At each iterate the SQP
evaluates them + their FD Jacobians and **linearises** them into the QP
subproblem (`c + Jc·d ≤ 0`, `ceq + Jceq·d = 0`), with an **L1-merit**
backtracking line search penalising the nonlinear-constraint violation. The
`[c, ceq] = nonlcon(x)` multi-output handle call is enabled by the
varargout / anonymous-multi-output support
([bugs/lang/anonymous-multi-output](../lang/anonymous-multi-output.md),
[multi-output-handle-call](../lang/multi-output-handle-call.md)). Verified vs
MATLAB R2025b: `min x1+x2 s.t. x1²+x2²≤1 → [−1/√2 −1/√2]`; `min x1²+x2² s.t.
x1+x2−2=0 (ceq) → [1 1]`.

## References
- `src/bundle/src/register/optim/fmincon_reg.cpp` (`kFminconMSource`),
  `optim_library.cpp` (registration)
- reused: the `quadprog` active-set QP subproblem
- `tools/parity/specs/fmincon.json`,
  `src/toolboxes/optim/tests/fmincon_test.cpp` (6 cases),
  `known_bugs_test.cpp` (`Fmincon`, promoted live),
  smoke `tests/smoke/fmincon_smoke.m`
- blocked extension: [bugs/lang/multi-output-handle-call](../lang/multi-output-handle-call.md)
- MATLAB `doc fmincon`
