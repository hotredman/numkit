# builtin/atan2d — ТЗ for completion

**Status:** closed
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

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Transcendentals + rounding spec-extension batch (14 funcs).
  All libm-backed, bit-identical MATLAB R2025b. See libs/builtin/tests/
  transcendentals_batch_test.cpp + smoke + 14 parity specs.
