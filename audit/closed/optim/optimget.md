# optim/optimget — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 4fae461
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK` on
benched input.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (trivial-fix batch)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED with a vague "struct field-access syntax differs" note. Re-probed: optimget(opts, name) and optimget(opts, name, default) work bit-identically with MATLAB R2025b (verified on TolX retrieval and missing-field default fallback). Spec restored to real probe.
