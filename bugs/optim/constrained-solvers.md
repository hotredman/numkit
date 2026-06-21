# optim constrained/gradient solvers — cluster (all split out + FIXED)

- **Status:** ✅ FIXED (2026-06-19) — the whole cluster is implemented; each
  function now has its own file:
  - [fminunc.md](fminunc.md) — unconstrained BFGS minimizer
  - [quadprog.md](quadprog.md) — quadratic program (active-set)
  - [linprog.md](linprog.md) — linear program (proximal-QP)
  - [fmincon.md](fmincon.md) — constrained minimization (SQP over quadprog)
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Summary
This file was the cluster index for the four constrained / gradient
optimizers `fminunc` / `quadprog` / `linprog` / `fmincon`. All four landed
2026-06-19 as embedded-`.m` solvers built around the shipped `fzero` /
`fminsearch` pattern and a new `quadprog` active-set QP (which `linprog` and
`fmincon` both reuse). See the per-function files above for the algorithm,
parity, and any scope notes (notably: `linprog` returns the min-norm point on
a degenerate optimal face rather than MATLAB's vertex; `fmincon` defers
nonlinear constraints pending VM multi-output-handle support —
[bugs/lang/multi-output-handle-call](../lang/multi-output-handle-call.md)).

## Repro (all now resolved)
```matlab
fminunc(@(x) (x-3)^2, 0)                                       % 3
quadprog(eye(2), [-1 -1])                                      % [1; 1]
linprog([1 1], [-1 0; 0 -1], [-1; -1])                         % [1; 1]
fmincon(@(x) x(1)^2+x(2)^2, [1 1], [],[],[],[], [0 0],[2 2])   % [0; 0]
```

## References
- new files under `src/toolboxes/optim/src/...`
- MATLAB `doc fmincon`, `doc linprog`, `doc quadprog`, `doc fminunc`
