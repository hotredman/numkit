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

## Re-confirmed + spec polish -- 2026-05-09 (Phase 0a-2)

Spec restored to canonical form using `magic(8)` instead of the
inlined uint8 matrix workaround. Now that magic() is shipped (cycle
46, commit 71efbf02), the spec reads naturally and stays aligned
with the original audit ТЗ wording.

Re-probed: numkit & MATLAB both report sum(BW(:)) = 11. Octave does
not ship grayconnected; correctness=OK against MATLAB reference.

Status: NOT actually deferred -- the earlier "DEFERRED" mention is
past-tense narrative ("Initial closure was DEFERRED, then fixed").
