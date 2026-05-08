# builtin/allunique — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 6c0964f
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Output matches MATLAB on probed inputs.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases (empty
   inputs, integer-type variations). `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Misc spec-extension batch (predicates + airy + coord conversion).
  All bit-identical MATLAB R2025b. See misc_batch_test.cpp.
