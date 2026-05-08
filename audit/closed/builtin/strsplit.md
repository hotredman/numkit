# builtin/strsplit — ТЗ for completion

**Status:** closed
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

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: String-ops spec-extension batch (14 funcs). All bit-identical
  MATLAB R2025b. See strings_batch_test.cpp + smoke + 14 specs.
