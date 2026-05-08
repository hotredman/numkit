# builtin/ldivide — ТЗ for completion

**Status:** closed
**Priority:** medium (PROGRESS notes `correctness=MISMATCH` on bench)
**Effort:** small (joint with `rdivide`)
**Audited at commit:** 42e1ec3
**Audit date:** 2026-05-06

## Gaps

Same shape as `rdivide` (see `audit/findings/builtin/rdivide.md`)
— basic cases match, bench input flags MISMATCH; likely zero-division
edge handling.

## Recommended fixes

Joint with `rdivide`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Arithmetic-ops spec-extension batch (10 funcs). All bit-identical
  MATLAB R2025b on probed paths. See arith_batch_test.cpp.
