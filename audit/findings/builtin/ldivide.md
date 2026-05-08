# builtin/ldivide — ТЗ for completion

**Status:** open
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
