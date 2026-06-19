# optim.fmincon / linprog — missing

- **Status:** 🔴 OPEN (`fminunc` + `quadprog` split out + FIXED 2026-06-19 —
  see [fminunc.md](fminunc.md), [quadprog.md](quadprog.md))
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
The remaining constrained optimizers are not registered: `fmincon`
(nonlinear constrained) and `linprog` (linear program). (`fminunc` and
`quadprog` are now done — see their split-out files.)

## Repro
```matlab
fmincon(@(x) x(1)^2+x(2)^2, [1 1], [],[],[],[], [0 0],[2 2])  % undefined
linprog([1 1], [-1 0], [-1])                                   % undefined
```

## Root cause
Not implemented. (`fminsearch` / `fminbnd` / `fzero` / `lsqnonneg` / `fminunc`
/ `quadprog` exist.)

## Suggested fix
Remaining:
- `linprog`: simplex or interior-point LP. The optimum is a vertex (or a
  face for degenerate costs) — match MATLAB's solution; phase-1/phase-2
  simplex is the natural fit.
- `fmincon`: SQP/interior-point over `fminunc` + constraint handling
  (could reuse the `quadprog` active-set as the SQP subproblem solver).
✅ `fminunc` (BFGS) and `quadprog` (active-set QP) done. `linprog` next
(reuses no existing solver directly — needs a simplex), then `fmincon`
(SQP, hardest). Verify against MATLAB on small textbook problems.

## References
- new files under `src/toolboxes/optim/src/...`
- MATLAB `doc fmincon`, `doc linprog`, `doc quadprog`, `doc fminunc`
