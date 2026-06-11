# control.care / dare — algebraic Riccati equation solvers missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
The continuous and discrete algebraic Riccati solvers `care` and `dare`
are not registered. They are the standalone primitives that `lqr`/`dlqr`
(see control/lqr-hinfnorm.md) are built on, and are also used directly for
Kalman filtering and H∞ design.

## Repro
```matlab
X = care([0 1; 0 0], [0; 1], eye(2));
% MATLAB: trace(X)=3.46410161513776, X(1,1)=1.73205080756888 (=√3)
% numkit: Error — VM: undefined function 'care'
X = dare([1 1; 0 1], [0; 1], eye(2), 1);
% MATLAB: trace(X)=7.56025722770319
% numkit: Error — VM: undefined function 'dare'
```

## Root cause
Not implemented. Solving the (C)ARE needs the Hamiltonian / symplectic
eigenvector (or Schur) method — numkit already has eigen/Schur in core
linalg, so the missing piece is the Hamiltonian assembly + stable-subspace
extraction.

## Suggested fix
- `care(A,B,Q[,R,S,E])`: form the Hamiltonian `H=[A -BR⁻¹Bᵀ; -Q -Aᵀ]`,
  take the stable invariant subspace `[X1;X2]` (eigenvectors with Re<0 or
  ordered Schur), then `X = X2 X1⁻¹`. Return `X` (and optionally the gain
  `K=R⁻¹(BᵀX+Sᵀ)` and closed-loop poles `L`).
- `dare(A,B,Q,R)`: discrete symplectic-pencil counterpart;
  `K=(R+BᵀXB)⁻¹BᵀXA`.
Medium. These let `lqr`/`dlqr` become thin wrappers. Verify `X` (symmetric,
stabilising) vs MATLAB.

## References
- new file under `toolboxes/control/src/...`; cf. control/lqr-hinfnorm.md
- shipped: `lyap`/`dlyap`/`place`/`ss`; eigen/Schur in core linalg
- MATLAB `doc care`, `doc dare`
