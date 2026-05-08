# io/writematrix — ТЗ for completion

**Status:** open
**Priority:** medium (PROGRESS notes `correctness=MISMATCH`)
**Effort:** small
**Audited at commit:** 2e9b36e
**Audit date:** 2026-05-06

## Gaps

PROGRESS bench flags MISMATCH. Likely number formatting (precision,
trailing zeros) or delimiter convention.

## Recommended fixes

1. **Probe** the exact bench input, diff the file outputs char-by-char.
2. **Spec extension** after divergence is identified.

## Out of scope for this ТЗ

- N/A.
