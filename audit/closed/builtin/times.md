# builtin/times — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 42e1ec3
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** Output matches MATLAB on probed inputs.

## Recommended fixes

1. **Spec extension** — fingerprint with edge cases (negative inputs,
   special values). `tol = 0`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Arithmetic-ops spec-extension batch (10 funcs). All bit-identical
  MATLAB R2025b on probed paths. See arith_batch_test.cpp.
