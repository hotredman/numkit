# io/readlines — ТЗ for completion

**Status:** open
**Priority:** medium (PROGRESS notes `correctness=MISMATCH`)
**Effort:** small
**Audited at commit:** 2e9b36e
**Audit date:** 2026-05-06

## Gaps

PROGRESS bench (4-line file → string array) flags MISMATCH.
Likely line-ending or trailing-newline handling.

## Recommended fixes

1. **Probe** with \"\r\n\" vs \"\n\" line endings, trailing/no-trailing
   newline, empty lines. Compare against MATLAB.
2. **Spec extension** after divergence is identified.

## Out of scope for this ТЗ

- N/A.
