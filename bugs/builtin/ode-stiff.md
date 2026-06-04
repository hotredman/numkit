# builtin.ode15s (+ stiff/multistep ODE family) — solvers missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
The explicit solvers `ode45`/`ode23` are present, but the stiff and
multistep members of the MATLAB ODE suite are not: `ode15s` (stiff, NDF),
`ode23s` (stiff, Rosenbrock), `ode23t`/`ode23tb` (trapezoidal/TR-BDF2), and
`ode113` (variable-order Adams). Stiff problems silently have no working
solver.

## Repro
```matlab
[t, y] = ode15s(@(t,y) -y, [0 1], 1);
% MATLAB: y(1)=1, y(end)≈0.3683 (≈ e^-1, within solver tolerance)
% numkit: Error — VM: undefined function 'ode15s'
```

## Root cause
Not implemented. The non-stiff RK pair (`ode45`/`ode23`) exists, but the
implicit BDF/NDF and Rosenbrock integrators (which need a Jacobian + Newton
solve per step) are absent.

## Suggested fix
- `ode15s`: variable-order NDF/BDF with a Newton iteration using a
  finite-difference (or supplied `odeset('Jacobian',…)`) Jacobian and
  adaptive step/order control. Medium-large.
- `ode23s`: a single Rosenbrock step (cheaper, one Jacobian/step).
- `ode113`: Adams-Bashforth-Moulton PECE.
Reuse the existing `ode45` event/output/time-span plumbing and the linear
solver. Verify `y(end)` on a stiff decay vs MATLAB. Large overall — defer
unless requested; this entry tracks the gap.

## References
- new file(s) under `libs/builtin/src/...`; reuse the ode45 driver + a solve
- MATLAB `doc ode15s`, `doc ode23s`, `doc ode113`
