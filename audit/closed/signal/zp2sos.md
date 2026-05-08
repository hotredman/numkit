# signal/zp2sos — ТЗ for completion

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
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Signal batch 4 (TF + dB + xcorr + filter conversions, 15 funcs).
  Bit-identical MATLAB R2025b on probed inputs (14 verified, 1 deferred).
  See signal_batch4_test.cpp.
