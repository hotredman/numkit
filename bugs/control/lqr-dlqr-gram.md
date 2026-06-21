# control.lqr / dlqr / gram — optimal-control gain + gramians missing

- **Status:** ✅ FIXED (2026-06-19) — wrappers on care / dare / lyap
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

> Split note: this bug originally also tracked `hinfnorm` (H∞ norm), which
> needs a different algorithm (Hamiltonian imaginary-eigenvalue bisection).
> That piece is now its own entry — see control/hinfnorm.md (still OPEN).

## Symptom
A cluster of core Control-Toolbox functions were not registered: `lqr`
(LQR gain), `dlqr` (discrete LQR), and `gram` (controllability /
observability gramian).

## Repro
```matlab
K = lqr([0 1; 0 0], [0; 1], eye(2), 1)
% MATLAB: K = [1.0000  1.7321]
dlqr([0.9 0.1;0 0.8],[0;1],eye(2),1)     % MATLAB sum(K)=0.7100
gram(ss([-1 0;0 -2],[1;1],[1 1],0),'c')  % MATLAB sum=1.41667
```

## Root cause
Not implemented. Each, however, reduces to a solver numkit now has:
`lqr`/`dlqr` are the algebraic Riccati gain (care/dare — see
control/care-dare.md, fixed 2026-06-19); `gram` is a Lyapunov solve
(lyap/dlyap, already shipped).

## Fix (2026-06-19)
Thin wrappers — no new numerics:

- **lqr** (`riccati_reg.cpp`): `[K,S,P] = lqr(A,B,Q[,R])` calls `care` and
  re-orders its `{X, L, G}` to MATLAB's `[K=gain, S=Riccati solution,
  P=closed-loop poles]`. R defaults to identity.
- **dlqr** (`riccati_reg.cpp`): same, on `dare`.
- **gram** (`state.cpp`, reuses `pullABC`): `W = gram(sys, 'c'|'o')` solves
  the gramian Lyapunov equation — `'c'`: `A·Wc+Wc·Aᵀ+B·Bᵀ=0` → `lyap(A,
  B·Bᵀ)`; `'o'`: `Aᵀ·Wo+Wo·A+Cᵀ·C=0` → `lyap(Aᵀ, Cᵀ·C)`. Discrete systems
  (`Ts≠0`) route to `dlyap`.

Verified vs MATLAB R2025b (parity `lqr.json` / `dlqr.json` / `gram.json`
→ OK): lqr `K=[1, √3]`, `S(1,1)=√3`, poles `−0.866±0.5i`; dlqr
`sum(K)=0.71004388`, `K=[0.211338, 0.498706]`; gram `Wc=[0.5 0.3333;
0.3333 0.25]`, `sum=1.41667`, residual 0. Guards: `care_dare_test.cpp`
(`LqrGainSolutionPoles`, `DlqrGain`, `GramControllability`,
`GramObservability`, `GramBadTypeThrows`), `known_bugs_test.cpp` (`Lqr`,
`Dlqr`, `Gram`, promoted live); smoke `lqr_dlqr_gram_smoke.m`.

Deferred: lqr/dlqr cross-term `N` and the `lqr(sys,…)` form (the
`(A,B,Q[,R])` signature covers the common case).

## References
- `src/bundle/src/register/control/riccati/riccati_reg.cpp` (`lqr_reg`,
  `dlqr_reg`), `src/toolboxes/control/src/state/state.cpp` (`gram`),
  `.../include/numkit/control/state/state.hpp`,
  `src/bundle/src/register/control/state/state_reg.cpp` (`gram_reg`).
- `tools/parity/specs/{lqr,dlqr,gram}.json`.
- related: control/care-dare.md (lqr/dlqr wrap these), control/hinfnorm.md
  (the remaining piece of the original cluster)
- MATLAB `doc lqr`, `doc dlqr`, `doc gram`
