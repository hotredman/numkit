# control.lqr / control.hinfnorm — functions missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing functions)
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`lqr` (linear-quadratic regulator gain) and `hinfnorm` (H-infinity norm of
a system) are not registered.

## Repro
```matlab
K = lqr([0 1; 0 0], [0; 1], eye(2), 1)
% numkit: Error — VM: undefined function 'lqr'
% MATLAB: K = [1.0000  1.7321]
hinfnorm(ss([0 1;-1 0],[0;1],[1 0],0))
% numkit: Error — VM: undefined function 'hinfnorm'
```

## Root cause
Not implemented.

## Suggested fix
- `lqr(A,B,Q,R)`: solve the continuous-time algebraic Riccati equation
  (CARE) for `P`, then `K = R⁻¹ Bᵀ P`. Needs a CARE solver (Schur/Hamiltonian
  eigenvector method) — numkit already has eigen/Schur in core linalg.
  Medium. `dlqr` (discrete) is the sibling.
- `hinfnorm(sys)`: peak gain over frequency (largest singular value of the
  frequency response, refined by a bisection / Hamiltonian-matrix test).
  Medium. Could file separately if scoped apart from lqr.

Both are Control-Toolbox staples. Verify K / norm vs MATLAB.

## References
- new file(s) under `libs/control/src/...`
- shipped: `lyap`, `dlyap`, `place`, `care`? (check), `ss`, `tf`, `sigma`
- MATLAB `doc lqr`, `doc hinfnorm`
