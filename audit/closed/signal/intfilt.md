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
- Closed in commit: pending (refined defer)
- Closed date: 2026-05-09
- Notes: PARTIAL FIX. Output LENGTH now matches MATLAB (2*R*L - 1) and DC gain = R is preserved. Coefficient VALUES still differ -- numkit uses Hamming-windowed sinc; MATLAB uses a least-squares FIR design (likely firls with specific bands/weights). Closing fully requires implementing firls (~150 lines: Toeplitz normal equations on sampled desired response). Practical impact: numkit intfilt works correctly for interp() upsampling (gtest passes), just produces a different specific FIR than MATLAB.
