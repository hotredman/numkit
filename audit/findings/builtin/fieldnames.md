# builtin/fieldnames — ТЗ for completion

**Status:** open
**Priority:** medium (PROGRESS notes `correctness=MISMATCH`)
**Effort:** small
**Audited at commit:** 780c049
**Audit date:** 2026-05-06

## Gaps

PROGRESS bench (5-field struct cell-out) flags MISMATCH. Likely
field-name ordering or cell layout difference.

## Recommended fixes

1. **Probe** `fieldnames(struct('c',1,'a',2,'b',3))` to verify
   ordering. MATLAB returns insertion order.
2. **Spec extension** after divergence is identified.

## Out of scope for this ТЗ

- N/A.
