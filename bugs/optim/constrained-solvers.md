# optim.fmincon / linprog / quadprog — missing

- **Status:** 🔴 OPEN (`fminunc` split out + FIXED 2026-06-19 — see
  [fminunc.md](fminunc.md))
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
The constrained optimizers are not registered: `fmincon` (nonlinear
constrained), `linprog` (linear program), `quadprog` (quadratic program).
(`fminunc` — unconstrained gradient — is now done; see
[fminunc.md](fminunc.md).)

## Repro
```matlab
fmincon(@(x) x(1)^2+x(2)^2, [1 1], [],[],[],[], [0 0],[2 2])  % undefined
linprog([1 1], [-1 0], [-1])                                   % undefined
quadprog(eye(2), [-1 -1])                                      % undefined
```

## Root cause
Not implemented. (`fminsearch` / `fminbnd` / `fzero` / `lsqnonneg` / now
`fminunc` exist.)

## Suggested fix
Substantial — these need real QP/LP/SQP machinery:
- `quadprog`: active-set or interior-point QP.
- `linprog`: simplex or interior-point LP.
- `fmincon`: SQP/interior-point over `fminunc` + constraint handling.
Large; likely several separate items. ✅ `fminunc` done first (BFGS, the
smallest). Next: `quadprog`/`linprog`, then `fmincon`. Verify against MATLAB
on small textbook problems.

## References
- new files under `src/toolboxes/optim/src/...`
- MATLAB `doc fmincon`, `doc linprog`, `doc quadprog`, `doc fminunc`
