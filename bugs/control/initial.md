# control.initial — initial-condition response missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`initial(sys, x0)` — the time response of a state-space system to an
initial state `x0` with zero input — is not registered. numkit ships
`step`/`impulse`/`lsim` but not the initial-condition response.

## Repro
```matlab
[y, t] = initial(ss(-2, 0, 1, 0), 1);
% MATLAB: y(1) = 1, y(end) = 0.00301995172040398  (y = e^{-2t})
% numkit: Error — VM: undefined function 'initial'
```

## Root cause
Not implemented. The simulation kernel already exists (`lsim`/`step` solve
`ẋ=Ax+Bu`); `initial` is the same integrator with `u≡0` and `x(0)=x0`,
output `y = C x`.

## Suggested fix
Reuse the `lsim`/`step` state propagation: discretize `A` (matrix
exponential, already in `c2d`/`expm`), propagate `x` from `x0` with no
input, return `y=Cx` (+ `t`, `x`). If no time vector is given, auto-pick a
horizon from the system poles (same heuristic as `step`'s auto-time — see
DEFERRED GAP (A); a fixed `t` input works without it). Small once wired to
the existing simulator. Verify `y` decay vs MATLAB.

## References
- new file under `libs/control/src/...`; reuse `lsim`/`step`/`c2d` kernels
- MATLAB `doc initial`
