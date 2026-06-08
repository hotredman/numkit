# signal.spectrogram — missing 4th output (ps = power spectral density)

- **Status:** ✅ FIXED (1128db65, 2026-06)
- **Severity:** P2 (missing output)
- **Kind:** missing-output
- **Found:** 2026-06 via signal.* DEEP-PROBE sweep

## Symptom
`[s, f, t, ps] = spectrogram(...)` threw "Too many output arguments" — the
4th output `ps` (one-sided PSD per time/frequency bin) was missing.

## Repro (pre-fix)
```matlab
[s, f, t, ps] = spectrogram((1:64)', 8, 4, 16, 100);
% numkit (pre-fix): Error — Too many output arguments
% MATLAB: ps(1,1)=1.08212  ps(3,2)=1.98718  sum(ps(:))=3254.62
```

## Fix
Wired `ps` into `spectrogram_reg` before `s` is moved out:
`ps[k] = c[k]·|s[k]|² / (fs·Σwin²)`, `c = 2` on interior bins, `1` at DC and
(when present) Nyquist. Σwin² uses the supplied window, or replicates the
core default `hamming(floor(nx/4.5))` when none is given; `fs` defaults to
`2π`. Bit-identical to MATLAB for both the explicit-window+fs and the
default-window cases.

## References
- `toolboxes/signal/src/time_frequency/spectrogram.cpp` (spectrogram_reg)
- `tools/parity/specs/spectrogram_ps.json`
- `toolboxes/signal/tests/spectral_test.cpp` (SpectrogramPsd* tests)
