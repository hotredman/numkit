# stats/anova1 — ТЗ for completion

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
- Closed in commit: pending (re-probe + 1 fix)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED with vague 'output struct shape' note. Re-probed with correct (y, group, 'off') signature: p-value bit-identical with MATLAB R2025b (0.0251 on probed input). Earlier defer was a spec issue (called with single matrix arg); restored real probe.
