# signal.spectrogram — 5th/6th outputs (fc, tc) missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing outputs)
- **Kind:** missing-output
- **Found:** 2026-06-04 via DEEP-PROBE (multi-output sweep)

## Symptom
MATLAB `spectrogram` returns up to six outputs `[s, f, t, ps, fc, tc]`, where
`fc` and `tc` are the per-time-bin centre-of-mass frequency and time vectors.
numkit emits at most four (`s, f, t, ps`) and throws "Too many output
arguments" for `fc`/`tc`. (`ps`, the 4th output, was added earlier — see
bugs/signal/spectrogram-ps.md.)

## Repro
```matlab
x = sin(2*pi*0.1*(0:99));
[s, f, t, ps, fc, tc] = spectrogram(x, 16, 8, 16, 1);
% numkit: Error — Too many output arguments
% MATLAB: fc, tc are length-numel(t) vectors; here tc(1)=8, fc(1)≈0
```

## Root cause
`spectrogram_reg` stops wiring outputs at `ps` (index 4); the centroid
vectors `fc`/`tc` are not computed.

## Suggested fix
After the STFT, compute per-column centroids from the power spectrum `ps`:
`fc(k) = Σ_j f(j)·ps(j,k) / Σ_j ps(j,k)` and the analogous time centroid
`tc(k)` (per MATLAB's reassignment-style definition). Thread them as the 5th
and 6th outputs by `nargout`. Validate `fc`/`tc` vs MATLAB on a tone +
chirp. Moderate; reuses the `ps` already computed.

## References
- `src/toolboxes/signal/src/time_frequency/spectrogram.cpp` (spectrogram_reg)
- bugs/signal/spectrogram-ps.md (the ps 4th-output fix)
- MATLAB `doc spectrogram` (fc, tc)
