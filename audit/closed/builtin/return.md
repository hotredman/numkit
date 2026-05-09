# builtin/return — ТЗ for completion

**Status:** open
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
- Closed in commit: pending (cycle 41)
- Closed date: 2026-05-09
- Notes: DEFERRED (KNOWN GAP) — script-level `return` causes MATLAB`s run wrapper to error; numkit allows it. Placeholder spec keeps harness green.
