# signal/highpass — ТЗ for completion

**Status:** open
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
- Closed in commit: pending (refined defer note)
- Closed date: 2026-05-09
- Notes: DEFERRED (KNOWN GAP, refined). Earlier defer comment said "FAIL/MISMATCH". Real cause now identified: signature gap was fixed (default fs=2 like MATLAB), and freqz endpoint fix was applied. Remaining VALUE difference: numkit uses order-8 Butterworth + forward filter; MATLAB uses minimum-order FIR designed via firgr/firpm (steepness=0.85) + zero-phase filtfilt by default. Same output shape (numel matches), different values. To close: implement min-order FIR + filtfilt path in libs/signal/src/digital_filtering/spec_driven.cpp (significant work; depends on firgr/firpm not yet shipped).
