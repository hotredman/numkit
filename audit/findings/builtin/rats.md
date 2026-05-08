# builtin/rats — ТЗ for completion

**Status:** open
**Priority:** **high** (PROGRESS notes `correctness=MISMATCH`)
**Effort:** small
**Audited at commit:** f82f380
**Audit date:** 2026-05-06

## Gaps

PROGRESS marks `correctness=MISMATCH`. Probe was interrupted by
upstream rat error; needs dedicated re-probe.

## Recommended fixes

1. **Re-probe in isolation** — test `rats(0.5)`, `rats(pi)`,
   `rats(1/3)`, etc. against MATLAB. Document specific divergence.
2. **Spec extension** after divergence is identified.

## Out of scope for this ТЗ

- N/A.
