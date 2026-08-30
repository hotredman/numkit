# wavelet.wavedec2 / detcoef2 / appcoef2 — 2-D wavelet decomposition family missing

- **Status:** ✅ FIXED (2026-06-19) — iterated dwt2 + [C,S] bookkeeping
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

## Fix (2026-06-19)
New TU `dwt/multilevel2.cpp` (+ `multilevel2.hpp`, `multilevel2_reg.cpp`),
the 2-D analogue of how `multilevel.cpp` wraps `dwt`/`idwt`. Implemented
`wavedec2` + `waverec2` + `appcoef2` + `detcoef2` on top of the existing
single-level `dwt2`/`idwt2` — which already accept a wavelet name, so the
new **bior/rbio** families work in 2-D too.

- `wavedec2(X,N,wname)` → `[C,S]`: iterate `dwt2` N times on the running
  LL band; pack `C = [cA_N | cH_N cV_N cD_N | … | cH_1 cV_1 cD_1]`
  (coarsest-first, each band column-major) and the `(N+2)×2` size matrix
  `S` (`S(1,:)=size(cA_N)`, `S(i,:)`=level-`(N-i+2)` detail size,
  `S(N+2,:)=size(X)`). Key identity used by the extractors:
  `size(A_k)=S(N-k+2,:)`.
- `appcoef2(C,S,wname,k)`: `k==N` returns the stored `cA_N`; `k<N`
  reconstructs down through `idwt2` using the stored detail bands;
  `k==0` is the full image (= `waverec2`). Default `k`=coarsest.
- `detcoef2(type,C,S,k)`: slice the H/V/D band at level `k` (`'all'` →
  `[H,V,D]` in the register wrapper).
- `waverec2(C,S,wname)`: full reconstruction (`appcoef2` at level 0).

Verified vs MATLAB R2025b: db1 4×4 N=1 (numel(c)=16, c(1)=7, H(1,1)=−1,
V(1,1)=−4, A(2,2)=27), db2 8×8 N=2 (numel(c)=139, appcoef2 L2
A(1,1)=16.4557713660 4×4, detcoef2 h L1 H(1,1)=−0.8660254038 5×5, waverec2
err 2.6e−11 — matching MATLAB's own residual), non-square 5×3, bior2.2 2-D
round-trip. Parity `wavedec2.json` → OK.

## References
- `src/toolboxes/wavelet/src/dwt/multilevel2.cpp`,
  `include/numkit/wavelet/dwt/multilevel2.hpp`,
  `src/bundle/src/register/wavelet/dwt/multilevel2_reg.cpp` (+ library reg)
- `tools/parity/specs/wavedec2.json`,
  `src/toolboxes/wavelet/tests/wavedec2_test.cpp` (7 cases),
  `known_bugs_test.cpp` (`Wavedec2Family`, promoted live),
  smoke `tests/smoke/wavedec2_smoke.m`
- reused: single-level `dwt2`/`idwt2` + the 1-D `[C,L]` layout pattern
- MATLAB `doc wavedec2`, `doc detcoef2`, `doc appcoef2`
