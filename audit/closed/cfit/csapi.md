# cfit/csapi — ТЗ for completion

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
- Closed in commit: pending (re-probe batch)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED with vague 'struct field-access syntax differs' note. Re-probed: numkit csapi returns the standard pp-form struct {form, breaks, coefs, pieces, order, dim} matching MATLAB R2025b bit-identically (coefs(end,end)=16 on the canonical sqrt-x probe). Spec restored to real probe.
