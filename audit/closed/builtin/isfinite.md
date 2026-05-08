# builtin/isfinite — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 7a3e258
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK`.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge inputs and type
   conversions. `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Predicates2 + set-ops + format/transpose batch (26 funcs).
  All bit-identical MATLAB R2025b. See predicates2_batch_test.cpp.
