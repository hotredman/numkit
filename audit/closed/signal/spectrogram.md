# signal/spectrogram — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** d3d8da7
**Audit date:** 2026-05-06

## Gaps

**No major gap detected on basic call.** Numbers and shapes match
MATLAB on probed input.

## Recommended fixes

1. **Spec extension** — fingerprint over signal variants + roundtrip
   tests where applicable. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Signal batch 4 (TF + dB + xcorr + filter conversions, 15 funcs).
  Bit-identical MATLAB R2025b on probed inputs (14 verified, 1 deferred).
  See signal_batch4_test.cpp.
  KNOWN GAP: numkit's spectrogram default window/overlap/NFFT differ from MATLAB; output dimensions don't match. Documented as separate ТЗ.
