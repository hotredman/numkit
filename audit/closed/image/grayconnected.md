# image/grayconnected — ТЗ for completion

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
- Closed in commit: pending (parity spec fix)
- Closed date: 2026-05-09
- Notes: Initial closure (cycle 44) was DEFERRED -- but the function actually WORKS correctly. Re-probed with explicit magic(8) inline (since numkit doesn't ship magic()): bit-identical with MATLAB R2025b (sum(BW(:)) = 11 in both). The earlier defer was a parity-spec issue (the spec used magic() which numkit lacks), not a numkit bug. Spec restored to use explicit input.
