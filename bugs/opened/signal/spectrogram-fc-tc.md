# signal.spectrogram — 5th/6th outputs (fc, tc) missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing outputs)
- **Kind:** missing-output
- **Found:** 2026-06-04 via DEEP-PROBE (multi-output sweep)

## Symptom
MATLAB `spectrogram` returns up to six outputs `[s, f, t, ps, fc, tc]`, where
`fc` and `tc` are the **reassigned centre-of-energy frequency and time per STFT
cell** — matrices the SAME SIZE as `s` (not per-time-bin vectors; see the
re-scope note below). numkit emits at most four (`s, f, t, ps`) and throws "Too
many output arguments" for `fc`/`tc`. (`ps`, the 4th output, was added earlier —
see bugs/signal/spectrogram-ps.md.)

## Repro
```matlab
clear;
x = sin(2*pi*0.1*(0:99));
[s, f, t, ps, fc, tc] = spectrogram(x, 16, 8, 16, 1);
% numkit: Error — Too many output arguments
% MATLAB: fc, tc are length-numel(t) vectors; here tc(1)=8, fc(1)≈0
```

## Root cause
`spectrogram_reg` stops wiring outputs at `ps` (index 4); the centroid
vectors `fc`/`tc` are not computed.

## Suggested fix
**Re-scoped 2026-06-18 (probe correction):** `fc`/`tc` are NOT per-time-bin
centroid *vectors* — MATLAB returns them as **matrices the same size as `s`**
(one reassigned coordinate per STFT cell), i.e. the **reassignment method**, not
a simple column centroid. Probe: `[s,f,t,ps,fc,tc]=spectrogram(sin(2*pi*0.1*
(0:99)),16,8,16,1)` gives `numel(fc)=numel(tc)=99` (= `numel(s)` = 9×11), and
low-energy cells blow up (≈1e13) — the hallmark of per-cell reassignment, not a
bounded Σf·ps/Σps. So the original "per-column centroid" plan above is WRONG.

Correct fix is heavier: compute the reassigned frequency/time per cell via the
three-window STFT — analysis window `w`, time-ramped window `t·w`, and
frequency-derivative window `dw/dt`:
`tc = t - Re(S_tw·conj(S))/|S|²`, `fc = f + Im(S_dw·conj(S))/(2π|S|²)`
(MATLAB `signal.internal.spectral` reassignment). Validate vs MATLAB incl. the
degenerate low-energy cells. **Moderate→large, deferred** — needs the derivative
windows + exact MATLAB reassignment normalization, not the one-liner first
assumed.

## References
- `src/toolboxes/signal/src/time_frequency/spectrogram.cpp` (spectrogram_reg)
- bugs/signal/spectrogram-ps.md (the ps 4th-output fix)
- MATLAB `doc spectrogram` (fc, tc)
