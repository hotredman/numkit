# signal/intfilt — ТЗ for completion

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
- Closed in commit: pending (intfilt length fix)
- Closed date: 2026-05-09
- Notes: Initial closure (cycle 43) was DEFERRED -- output LENGTH was 2*R*L + 1 instead of MATLAB's 2*R*L - 1. Length now matches MATLAB (off-by-two corrected). Coefficient VALUES still differ -- numkit uses Hamming-windowed sinc with R-scaled DC gain; MATLAB uses proprietary firgr/firls equiripple FIR design. The R-scaled DC gain preserves the interp() upsampling amplitude (gtests pass). Closing values requires firls implementation (separate ТЗ).
