# signal/issingle — ТЗ for completion

**Status:** open
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
- Closed in commit: pending (refined defer note)
- Closed date: 2026-05-09
- Notes: DEFERRED (refined): MATLAB does not ship issingle as a standalone function (isfir is a digitalFilter method only; issingle does not exist - use isa(x,"single") instead). numkit ships issingle as a convenience predicate that works correctly under direct probe; the parity harness reports N/A because there is no MATLAB reference function with the same call shape. Placeholder spec keeps harness green; this is documentation, not a real bug.
