# control.lqr / hinfnorm / dlqr / gram — functions missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
A cluster of core Control-Toolbox functions are not registered: `lqr`
(LQR gain), `hinfnorm` (H-infinity norm), `dlqr` (discrete LQR), and `gram`
(controllability / observability gramian).

## Repro
```matlab
K = lqr([0 1; 0 0], [0; 1], eye(2), 1)
% numkit: Error — VM: undefined function 'lqr'
% MATLAB: K = [1.0000  1.7321]
hinfnorm(ss([0 1;-1 0],[0;1],[1 0],0))
% numkit: Error — VM: undefined function 'hinfnorm'
dlqr([0.9 0.1;0 0.8],[0;1],eye(2),1)     % undefined; MATLAB sum(K)=0.7100
gram(ss([-1 0;0 -2],[1;1],[1 1],0),'c')  % undefined; MATLAB sum=1.41667
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
- `dlqr(A,B,Q,R)`: discrete LQR via the DISCRETE algebraic Riccati equation
  (DARE); `K = (R + BᵀPB)⁻¹ BᵀPA`. Sibling of `lqr`.
- `gram(sys,'c'|'o')`: solve the (controllability / observability) Lyapunov
  equation — numkit already has `lyap`/`dlyap`, so this is mostly wiring.

All are Control-Toolbox staples. Verify K / norm / gramian vs MATLAB.

## References
- new file(s) under `toolboxes/control/src/...`
- shipped: `lyap`, `dlyap`, `place`, `care`? (check), `ss`, `tf`, `sigma`
- MATLAB `doc lqr`, `doc hinfnorm`
