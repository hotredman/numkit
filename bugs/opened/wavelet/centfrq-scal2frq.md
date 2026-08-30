# wavelet.centfrq / scal2frq — wavelet center-frequency helpers missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`centfrq` (center frequency of a wavelet) and `scal2frq` (convert CWT scales
to pseudo-frequencies) are not registered. These are the scale↔frequency
mapping helpers a CWT (see wavelet/cwt.md) and scalogram plotting need.

## Repro
```matlab
clear; import compat.*;
centfrq('db4')         % MATLAB: 0.714285714285714  (= 5/7)
scal2frq(4, 'db4', 1)  % MATLAB: 0.178571428571429  (= centfrq/(a·Δ) = 0.7143/4)
% numkit (each): Error — VM: undefined function 'centfrq'/'scal2frq'
```

## Root cause
Not implemented. `centfrq` estimates the dominant frequency of the wavelet
(by FFT of the wavelet function, or table lookup); `scal2frq(a,wname,Δ)` is
just `centfrq(wname) / (a·Δ)`.

## Suggested fix
- `centfrq(wname)`: build the wavelet function on a fine grid (the shape
  functions `dbwavf`/`morlet`/… are shipped), FFT it, take the frequency of
  the peak magnitude. Small.
- `scal2frq(a,wname,Δ)`: `f = centfrq(wname) ./ (a .* Δ)` — trivial once
  `centfrq` exists. Verify `centfrq('db4')==5/7` and the scale mapping vs
  MATLAB.

## ⚠️ Probe finding (2026-06-19) — needs a correct wavefun cascade
A naive reconstruction (FFT-peak of the wavelet function) does **not** match
MATLAB on any wavelet: orthogonal wavelets need the wavelet function `psi`
built by the **two-scale cascade** from the reconstruction filters
(`wfilters`/`dbwavf` are shipped, but numkit has **no `wavefun`/cascade**
yet); a hand cascade gave a near-Nyquist peak (wrong) — the upsample +
normalization details matter. The analytic cases were also off (numkit's
`morlet`→0.75 / `mexihat`→0.20 vs MATLAB 0.8125 / 0.25), so MATLAB's
specific grid/discretization is load-bearing. Reference values are clean
rationals (db4 5/7, db2 2/3, coif2 8/11; support = 2N−1) = `peak_bin /
support`, but reproducing the integer `peak_bin` needs the correct `psi`.
**Blocker:** implement a `wavefun` cascade (orthogonal `psi` from the
filters) first, then `centfrq = (peak_bin−1)/T` (with FFT-fold),
`scal2frq = centfrq/(a·Δ)`. MATLAB `centfrq.m` source must NOT be read
(IP) — reconstruct from behavior + wavelet theory.

## References
- new file under `src/toolboxes/wavelet/src/...`; reuse the wavelet shape functions
- blocked on: a `wavefun` cascade (orthogonal `psi` from `wfilters`)
- MATLAB `doc centfrq`, `doc scal2frq`
