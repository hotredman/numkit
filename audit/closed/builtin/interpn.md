# builtin/interpn — ТЗ for completion

**Status:** closed
**Priority:** low
**Effort:** small
**Audited at commit:** 3cb06a1
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK`.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (cycle 6)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED -- same root cause as interp3. Fixed by the same readGridAxis rewrite. interpn dispatches to interp3 internally; bit-identical with MATLAB R2025b on ndgrid form.
