# builtin/join — ТЗ for completion

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
- Notes: Misc batch 5 (poly + string-extras2 + math2 + error-handling, 19 funcs).
  Bit-identical MATLAB R2025b. See misc5_batch_test.cpp.
