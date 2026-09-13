# signal.pspectrum — function missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-09-12 via the Play gallery capability sweep (DSP
  section scripts wanted it; spectrogram/pwelch were used instead).

## Symptom

`pspectrum` is not registered — no single-function spectrum
reassignment / persistency view alongside the shipped
`spectrogram` / `pwelch`.

## Repro

```matlab
clear;
rng(1);
[P, F] = pspectrum(randn(1024, 1));
size(P)
% nk: Error — VM: undefined function 'pspectrum'
% MATLAB: ans = 4096 1
```

## Root cause

Not implemented.

## Suggested fix

Adaptive-resolution spectrogram (MATLAB's pspectrum: Welch-like
segmentation with Kaiser window, octave-band frequency reassignment,
log-spaced frequencies by default). Reuses pwelch's segmenting and
the FFT layer; the novel parts are the reassignment grid and the
two-sided power normalization. Validate `[P, F]` shapes + `db`
conversion against MATLAB on `randn` + chirp fixtures.

## References

- **Guard:** deferred — missing-fn entry (feature gap, not a defect)
- tracked in `bugs/missing.md` (Transforms / Nonparametric Spectral
  Estimation / Time-Frequency Analysis sections)
- shipped neighbours: `spectrogram`, `pwelch`, `fft`
- MATLAB `doc pspectrum`
