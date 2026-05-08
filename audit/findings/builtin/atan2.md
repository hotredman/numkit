# builtin/atan2 — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** a6e4264
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Two-arg quadrant-aware atan; element-wise
output matches MATLAB on probed input grid.

## Recommended fixes

1. **Spec extension** — quadrant grid fingerprint. `tol = 0`.

## Out of scope for this ТЗ

- N/A.
