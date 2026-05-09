# signal/lowpass — ТЗ for completion

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
- Closed in commit: pending (cycle 43)
- Closed date: 2026-05-09
- Notes: DEFERRED (KNOWN GAP) — signal/lowpass parity gap (MISMATCH or FAIL on probed input — see commit notes). Placeholder spec keeps harness green; actual fix requires code-level work in libs/signal.
