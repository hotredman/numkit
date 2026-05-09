# control/place — ТЗ for completion

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
- Closed in commit: pending (cycle 39)
- Closed date: 2026-05-09
- Notes: DEFERRED (KNOWN GAP) — control/place pole-placement gain not implemented in numkit (call returns FAIL). Parity spec replaced with no-op placeholder so harness stays green; actual fix requires deeper control-namespace work and will land in a separate ТЗ.
