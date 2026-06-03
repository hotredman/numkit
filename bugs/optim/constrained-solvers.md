# optim.fmincon / linprog / quadprog / fminunc — missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
The constrained / gradient optimizers are not registered: `fmincon`
(nonlinear constrained), `linprog` (linear program), `quadprog` (quadratic
program), `fminunc` (unconstrained gradient).

## Repro
```matlab
fmincon(@(x) x(1)^2+x(2)^2, [1 1], [],[],[],[], [0 0],[2 2])  % undefined
linprog([1 1], [-1 0], [-1])                                   % undefined
quadprog(eye(2), [-1 -1])                                      % undefined
fminunc(@(x) (x-3)^2, 0)                                       % undefined
```

## Root cause
Not implemented. (`fminsearch` / `fminbnd` / `fzero` / `lsqnonneg` exist.)

## Suggested fix
Substantial — these need real QP/LP/SQP machinery:
- `quadprog`: active-set or interior-point QP.
- `linprog`: simplex or interior-point LP.
- `fminunc`: BFGS/trust-region with FD gradient.
- `fmincon`: SQP/interior-point over `fminunc` + constraint handling.
Large; likely several separate items. File here as a known cluster; tackle
`fminunc` first (smallest), then `quadprog`/`linprog`, then `fmincon`.
Verify against MATLAB on small textbook problems.

## References
- new files under `libs/optim/src/...`
- MATLAB `doc fmincon`, `doc linprog`, `doc quadprog`, `doc fminunc`
