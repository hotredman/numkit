# stats.descriptive/datastats — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 015c30d
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK`.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (trivial-fix batch)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED with vague "struct field-access" note. Re-probed: numkit datastats returns a struct with fields {min, max, mean, median, num, range, std} matching MATLAB on COLUMN vector input. MATLAB requires column input (errors on row); numkit is more lenient. Spec uses column input; bit-identical.
