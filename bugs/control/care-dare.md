# control.care / dare — algebraic Riccati equation solvers missing

- **Status:** ✅ FIXED (2026-06-19) — matrix sign-function method
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
Not implemented. Solving the (C)ARE needs the stable invariant subspace of
the Hamiltonian (continuous) / symplectic pencil (discrete).

## Fix (2026-06-19)
Implemented `numkit::control::care` / `dare` (`riccati.cpp`) by the **matrix
sign-function method** — no Schur ordering, only `inv` + a small
least-squares solve, so it builds purely on toolboxes/control's existing
`internal::solveInPlace` LU kernel (the general Schur from
linalg/schur-nonsymmetric is *not* required for this realization).

- **care:** form the Hamiltonian `H = [A −BR⁻¹Bᵀ; −Q −Aᵀ]`, drive it to
  `S = sign(H)` by the scaled Newton iteration `Z ← ½(cZ + (cZ)⁻¹)`,
  `c = √(‖Z⁻¹‖_F/‖Z‖_F)` (Higham). `sign(H)` is a projector onto the stable
  invariant subspace; the read-off `W1=[Z12; Z22+I]`, `W2=−[Z11+I; Z21]`,
  `X = W1\W2` (least squares), `X ← ½(X+Xᵀ)` gives the stabilizing solution.
- **dare:** no Hamiltonian exists, so build the symplectic matrix and apply a
  **Cayley transform** `C = (S−I)(S+I)⁻¹` (maps the unit disk → left
  half-plane), then reuse the same sign machinery + read-off. Requires `A`
  nonsingular (the explicit symplectic form needs `A⁻¹`); the singular-`A`
  pencil path would need a QZ solver — documented gap, throws a clear error.

Output order matches MATLAB `[X, L, G]`: solution, closed-loop eigenvalues
`eig(A−BG)`, and gain (`G=R⁻¹BᵀX` for care; `G=(R+BᵀXB)⁻¹BᵀXA` for dare).
`L` is computed via `charPoly(A−BG)` → `math::roots` (the `pole` path). `R`
defaults to the identity for the 3-argument `care(A,B,Q)` / `dare(A,B,Q)`.

Verified vs MATLAB R2025b (parity `care.json` / `dare.json` → OK):
care `X(1,1)=√3=1.73205080756888`, `trace=3.46410161513776`, gain
`[1, √3]`, poles `−0.866±0.5i`, ARE residual ~4e-16; care2
(`[-3 2;1 1]`,R=3) `trace=9.9268254221`; dare `X(1,1)=2.94712296779058`,
`trace=7.56025722770319`, `|poles|=0.4221` (inside unit circle), residual
~3e-15. Guards: `care_dare_test.cpp` (8 TEST_F: solution / residual /
gain+poles / non-default R / singular-A throw, both equations),
`known_bugs_test.cpp` (`CareDare`, promoted live); smoke
`care_dare_smoke.m`.

These let `lqr`/`dlqr` become thin wrappers (still open — see
control/lqr-hinfnorm.md).

## References
- `src/toolboxes/control/src/riccati/riccati.cpp` (`care`, `dare`, scaled
  Newton `matsign`, `subspaceToX`), `.../include/numkit/control/riccati/riccati.hpp`,
  `src/bundle/src/register/control/riccati/riccati_reg.cpp` (`care_reg`,
  `dare_reg`; default-R + `[X,L,G]` emit).
- `tools/parity/specs/care.json`, `tools/parity/specs/dare.json`.
- related: control/lqr-hinfnorm.md (lqr/dlqr can now wrap these),
  linalg/schur-nonsymmetric.md (sibling Schur kernel — not needed here)
- MATLAB `doc care`, `doc dare`
