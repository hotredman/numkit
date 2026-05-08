# builtin/strsplit — ТЗ for completion

**Status:** open
**Priority:** medium (PROGRESS notes `correctness=MISMATCH`)
**Effort:** small
**Audited at commit:** c1fdebe
**Audit date:** 2026-05-06

## Gaps

PROGRESS bench (3.5k string, 500 splits → cell) flags MISMATCH.
Probe needed to identify the divergent case (likely empty-token
handling or trailing-delimiter convention).

## Recommended fixes

1. **Probe empty-token cases**: `strsplit(',a,,b,')` etc., compare
   against MATLAB.
2. **Spec extension** after divergence is identified.

## Out of scope for this ТЗ

- N/A.
