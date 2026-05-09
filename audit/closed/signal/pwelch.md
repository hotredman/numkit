# signal/pwelch — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 4fae461
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK` on
benched input.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (DSP-default batch)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED with vague "NFFT/window default" note. Two fixes applied:
  1. Added a fs parameter (default 2*pi, MATLAB convention) to all five Welch-family estimators (and periodogram). PSD scaling changed from 1/(winPower*nfft) to 1/(winPower*fs); the returned frequency vector now spans [0, fs/2] instead of [0, pi].
  2. Default window length changed from min(nx, 256) to floor(nx/4.5) for pwelch / cpsd / mscohere / tfestimate / spectrogram -- this gives 8 Hamming-windowed segments with 50% overlap, matching MATLAB.
Verified bit-identical with MATLAB R2025b on x = sin(2*pi*0.1*(0:255)) (sum(p)=20.29 for periodogram, 20.37 for pwelch/cpsd, mscohere=62.85, tfestimate=81.31, spectrogram returns 8 segments).
