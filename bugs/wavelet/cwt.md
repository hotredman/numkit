# wavelet.cwt — continuous wavelet transform missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`cwt(x)` — the continuous wavelet transform (default: analytic Morse
wavelet, returns complex coefficients over a log-spaced scale/frequency
grid) — is not registered. numkit ships the *discrete* transforms
(`dwt`/`wavedec`/`swt`/`modwt`) and the analyzing-wavelet shape functions
(`morlet`/`mexihat`/`cmorwavf`/`morsewavf`-style helpers) but not the CWT
itself.

## Repro
```matlab
x = [1 2 3 4 5 6 7 8 7 6 5 4 3 2 1 0];
cfs = cwt(x);
% MATLAB: complex, size = [11 16]  (11 scales x 16 time samples)
[cfs, f] = cwt(x);
% MATLAB: numel(f) = 11  (one frequency per scale)
% numkit (both): Error — VM: undefined function 'cwt'
```

## Root cause
Not implemented. The CWT engine (filter-bank construction over a
log-spaced scale grid, FFT-based convolution per scale, the scale↔frequency
mapping, and the default Morse-wavelet parameterisation) is absent.

## Suggested fix
Large. Build the default Morse (or Morlet) filter bank over a
voices-per-octave log-scale grid, transform via FFT-domain multiplication
per scale, return the complex coefficient matrix `[nScales × N]` plus the
frequency vector `f`. The analyzing-wavelet shapes exist already; the
missing parts are the scale-grid / frequency-mapping defaults and the
FFT-bank driver. Defer unless requested — verify the coefficient matrix
size + a couple of magnitudes vs MATLAB, mindful that the exact default
wavelet parameters must match.

## References
- new file under `libs/wavelet/src/...`; reuse FFT + the wavelet shape fns
- MATLAB `doc cwt`
