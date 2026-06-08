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

## References
- new file under `toolboxes/wavelet/src/...`; reuse the wavelet shape functions
- MATLAB `doc centfrq`, `doc scal2frq`
