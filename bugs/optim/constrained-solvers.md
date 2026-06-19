# optim.fmincon — missing

- **Status:** 🔴 OPEN (`fminunc` + `quadprog` + `linprog` split out + FIXED
  2026-06-19 — see [fminunc.md](fminunc.md), [quadprog.md](quadprog.md),
  [linprog.md](linprog.md))
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
The last constrained optimizer is not registered: `fmincon` (general
nonlinear constrained minimization). (`fminunc`, `quadprog` and `linprog`
are now done — see their split-out files.)

## Repro
```matlab
fmincon(@(x) x(1)^2+x(2)^2, [1 1], [],[],[],[], [0 0],[2 2])  % undefined
```

## Root cause
Not implemented. (`fminsearch` / `fminbnd` / `fzero` / `lsqnonneg` / `fminunc`
/ `quadprog` / `linprog` exist.)

## Suggested fix
`fmincon`: SQP or interior-point over `fminunc` + constraint handling. The
natural build is **SQP** reusing the shipped `quadprog` as the QP subproblem
solver: at each iterate linearise the constraints and solve a QP for the
step, with a line search / merit function. The FnHandle objective +
(optional) nonlinear-constraint handle run as bytecode (pausable), like the
other embedded-.m solvers. Hardest of the cluster (constraint linearization,
merit function, convergence). Verify against MATLAB on small textbook
problems (e.g. quadratic objective with bounds / linear constraints).

## References
- new files under `src/toolboxes/optim/src/...`
- MATLAB `doc fmincon`, `doc linprog`, `doc quadprog`, `doc fminunc`
