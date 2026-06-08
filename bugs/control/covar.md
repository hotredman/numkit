# control.covar — output covariance from white-noise input missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`covar(sys, W)` — the steady-state output covariance of a system driven by
white noise of intensity `W` — is not registered.

## Repro
```matlab
P = covar(ss(-1, 1, 1, 0), 1);
% MATLAB: P = 0.5
% numkit: Error — VM: undefined function 'covar'
```

## Root cause
Not implemented. `covar` solves the same Lyapunov equation numkit already
exposes as `lyap`/`dlyap`, so the missing piece is just the
`P = C·Q·Cᵀ (+ D·W·Dᵀ)` wiring on top of the controllability gramian
`A·Q + Q·Aᵀ + B·W·Bᵀ = 0`.

## Suggested fix
- Continuous: solve `A Q + Q Aᵀ + B W Bᵀ = 0` (via `lyap`, shipped), then
  output covariance `P = C Q Cᵀ` (the `D` term is ∞ for continuous
  white-noise unless `D=0`). Discrete: `dlyap` + `P = C Q Cᵀ + D W Dᵀ`.
Small — mostly gramian wiring. Verify `P` vs MATLAB on a 1st-order plant
(closed form `P = B²W/(2|a|)·C²`).

## References
- new file under `toolboxes/control/src/...`; reuse `lyap`/`dlyap`/`gram`
- MATLAB `doc covar`
