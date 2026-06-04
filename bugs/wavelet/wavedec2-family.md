# wavelet.wavedec2 / detcoef2 / appcoef2 — 2-D wavelet decomposition family missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
The 2-D multilevel wavelet decomposition family is not registered:
`wavedec2` (decompose an image), `detcoef2` (extract horizontal/vertical/
diagonal detail coefficients), and `appcoef2` (extract / reconstruct the
approximation). numkit ships the **1-D** counterparts
(`wavedec`/`detcoef`/`appcoef`) and the single-level 2-D `dwt2`, but not the
multilevel 2-D bookkeeping.

## Repro
```matlab
[c, s] = wavedec2(reshape(1:16,4,4), 1, 'db1');
% MATLAB: numel(c)=16, s(1,1)=2, c(1)=7
H = detcoef2('h', c, s, 1);     % MATLAB: 2x2, H(1,1)=-1
A = appcoef2(c, s, 'db1', 1);   % MATLAB: 2x2, A(1,1)=7
% numkit (each): Error — VM: undefined function 'wavedec2'/'detcoef2'/'appcoef2'
```

## Root cause
Not implemented. `dwt2` exists (one level), so `wavedec2` is the recursive
driver over the approximation sub-band plus the `[c, s]` coefficient/size
bookkeeping vector; `detcoef2`/`appcoef2` are the slice extractors into that
layout (the 2-D analogue of the shipped 1-D `detcoef`/`appcoef`).

## Suggested fix
- `wavedec2(X,N,wname)`: iterate `dwt2` N times on the LL sub-band; pack
  `[cA_N, cH_N,cV_N,cD_N, …, cH_1,cV_1,cD_1]` into `c` with the size matrix
  `s`. Medium.
- `detcoef2(type,c,s,k)`: return the H/V/D (or all) sub-band at level `k`.
- `appcoef2(c,s,wname,k)`: extract level-N approximation, reconstruct down
  to level `k` via `idwt2` if `k<N`.
Verify `c(1)`, sub-band sizes, and a couple of detail values vs MATLAB.

## References
- new file(s) under `libs/wavelet/src/...`; reuse `dwt2`/`idwt2` + 1-D layout
- MATLAB `doc wavedec2`, `doc detcoef2`, `doc appcoef2`
